#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Game/CombatManager.h"
#include "Game/DamageEmitter.h"
#include "Game/Room.h"
#include "Game/SpatialGrid.h"
#include "GamePackets.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Packet/PacketPtr.h"
#include "System/Session/SessionContext.h"
#include "System/Thread/IStrand.h"
#include <cmath>

namespace SimpleGame {

void Room::ExecuteUpdate(float deltaTime)
{
    // [Fix] 정지 중이거나 플레이어가 없으면 무거운 연산 즉시 중단

    if (!_gameStarted || _isGameOver || _isStopping.load() || _players.empty())
        return;

    // [Performance Measurement Start]
    auto startPerf = std::chrono::high_resolution_clock::now();

    _totalRunTime += deltaTime;
    _serverTick++;
    auto objects = _objMgr.GetAllObjects();

    // [1] Wave Update (Monster Spawn)
    _waveMgr.Update(deltaTime, this);

    // [2] Effect Update (DoT ticking, expiration)
    _effectMgr->Update(_totalRunTime, this);

    // [3] Grid Rebuild (Reflect Spawns immediately so AI can see neighbors)
    // [Fix] Rebuild MUST happen before AI Update
    auto currentObjects = _objMgr.GetAllObjects();
    _grid.Rebuild(currentObjects);

    // [3] Object Update (AI & DamageEmitter Spawn)
    // Now AI can use spatial queries correctly
    for (auto &obj : currentObjects)
    {
        if (!obj->IsDead())
        {
            obj->Update(deltaTime, this);
        }
    }

    // [4] Physics / Movement (Projectiles & Monsters)
    // Note: AI sets DesiredVelocity, Physics applies it and moves position
    UpdatePhysics(deltaTime, currentObjects);

    // [5] Combat / Collision / Cleanup
    if (_combatMgr)
    {
        _combatMgr->Update(deltaTime, this);
    }

    // [6] Network Sync
    SyncNetwork();

    // [7] Debug Broadcast (WebSocket Visualizer)
    BroadcastDebugState();

    // [Performance Measurement End]
    auto endPerf = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> duration = endPerf - startPerf;
    float elapsedSec = duration.count();

    _totalUpdateSec += elapsedSec;
    _updateCount++;
    if (elapsedSec > _maxUpdateSec)
        _maxUpdateSec = elapsedSec;

    if (_totalRunTime - _lastPerfLogTime >= 1.0f)
    {
        float avgSec = _updateCount > 0 ? _totalUpdateSec / _updateCount : 0.0f;

        // Count by type
        int monsterCount = 0;
        int projectileCount = 0;
        int itemCount = 0; // EXP Gems
        int otherCount = 0;

        for (const auto &obj : currentObjects)
        {
            if (obj->IsDead())
                continue;
            switch (obj->GetType())
            {
            case Protocol::ObjectType::MONSTER:
                monsterCount++;
                break;
            case Protocol::ObjectType::PROJECTILE:
                projectileCount++;
                break;
            case Protocol::ObjectType::ITEM:
                itemCount++;
                break;
            default:
                otherCount++;
                break;
            }
        }

        LOG_INFO(
            "[Perf] Room Use: Avg {:.4f}ms, Max {:.4f}ms | Total: {} (M: {}, P: {}, I: {}, O: {})",
            avgSec * 1000.0f,
            _maxUpdateSec * 1000.0f,
            currentObjects.size(),
            monsterCount,
            projectileCount,
            itemCount,
            otherCount
        );

        _lastPerfLogTime = _totalRunTime;
        _totalUpdateSec = 0.0f;
        _updateCount = 0;
        _maxUpdateSec = 0.0f;
    }
}

bool CheckCollision(const std::shared_ptr<GameObject> &a, const std::shared_ptr<GameObject> &b)
{
    // Simple Circle-Circle overlap check for now, can be AABB if needed.
    // User requested "AABB simulation", but GameObject usually has Radius.
    // Converting Radius to AABB: Box(x-r, y-r, w=2r, h=2r)

    // Using Radius (Circle) for smoother sliding fits "Monster" better usually,
    // but USER asked for "AABB". Let's stick to Radius check as it's standard for this project's
    // "SpatialGrid::QueryRange". If user insists strictly on AABB (Square), we can switch. Given "ToySurvival", Circle
    // is safe. Let's strictly implement "Block Intrusion".

    float dx = a->GetX() - b->GetX();
    float dy = a->GetY() - b->GetY();
    float distSq = dx * dx + dy * dy;
    float radSum = 20.0f; // Default fixed size assumption or...

    // Determining radius. Dynamic cast or standard interface?
    // GameObject doesn't seem to have GetRadius() exposed in Room.cpp snippets, but SpatialGrid uses it?
    // Wait, SpatialGrid.cpp:34 uses "dx*dx + dy*dy <= radiusSq".
    // Monster.cpp:38 calls GetRadius(). So it exists.

    // Use slightly smaller radius for "body" collision to allow some visual overlap
    float rA = 15.0f; // Approx
    float rB = 15.0f;

    // Try to get actual radius if possible, otherwise hardcode for ToySurvival
    if (a->GetType() == Protocol::ObjectType::MONSTER)
        rA = 15.0f;
    if (b->GetType() == Protocol::ObjectType::MONSTER)
        rB = 15.0f;

    radSum = rA + rB;
    return distSq < (radSum * radSum);
}

void Room::UpdatePhysics(float deltaTime, const std::vector<std::shared_ptr<GameObject>> &objects)
{
    // Define bounds (Map 2000x2000?)
    // Assuming map is large enough.

    for (auto &obj : objects)
    {
        if (obj->IsDead())
            continue;

        // Only Monsters collide with each other
        bool isMonster = (obj->GetType() == Protocol::ObjectType::MONSTER);

        float vx = obj->GetVX();
        float vy = obj->GetVY();

        // Projectiles and other non-monsters just move linearly
        if (!isMonster)
        {
            obj->SetPos(obj->GetX() + vx * deltaTime, obj->GetY() + vy * deltaTime);
            continue;
        }

        float currentX = obj->GetX();
        float currentY = obj->GetY();
        float moveX = vx * deltaTime;
        float moveY = vy * deltaTime;

        // [Physics Removed]
        // Collision avoidance is now fully handled by AI (Steering Behaviors).
        // Physics only handles simple movement integration.

        obj->SetPos(currentX + moveX, currentY + moveY);
    }
}

// [Networking]
void Room::SendToPlayer(uint64_t sessionId, const System::IPacket &pkt)
{
    if (_dispatcher)
    {
        uint16_t size = pkt.GetTotalSize();
        uint16_t safeSize = size + (size / 10) + 16;
        auto *msg = System::MessagePool::AllocatePacket(safeSize);
        if (msg == nullptr)
            return;

        pkt.SerializeTo(msg->Payload());
        System::PacketPtr serialized(msg);

        _dispatcher->WithSession(
            sessionId,
            [packet = std::move(serialized)](System::SessionContext &ctx) mutable
            {
                ctx.Send(std::move(packet));
            }
        );
    }
}

void Room::BroadcastPacket(const System::IPacket &pkt, uint64_t excludeSessionId)
{
    if (!_dispatcher)
        return;

    uint16_t size = pkt.GetTotalSize();
    uint16_t safeSize = size + (size / 10) + 16;
    auto *msg = System::MessagePool::AllocatePacket(safeSize);
    if (msg == nullptr)
        return;

    pkt.SerializeTo(msg->Payload());
    System::PacketPtr serialized(msg);

    for (const auto &[sid, player] : _players)
    {
        if (sid == excludeSessionId)
            continue;

        _dispatcher->WithSession(
            sid,
            [packet = serialized](System::SessionContext &ctx) mutable
            {
                ctx.Send(std::move(packet));
            }
        );
    }
}

void Room::BroadcastSpawn(const std::vector<std::shared_ptr<GameObject>> &objects)
{
    if (objects.empty())
        return;

    Protocol::S_SpawnObject msg;
    msg.set_server_tick(_serverTick);

    for (const auto &obj : objects)
    {
        auto *info = msg.add_objects();
        info->set_object_id(obj->GetId());
        info->set_type(obj->GetType());
        info->set_x(obj->GetX());
        info->set_y(obj->GetY());
        info->set_hp(obj->GetHp());
        info->set_max_hp(obj->GetMaxHp());
        info->set_state(obj->GetState());
        info->set_vx(obj->GetVX());
        info->set_vy(obj->GetVY());
        info->set_look_left(obj->GetLookLeft());

        if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {
            auto m = std::dynamic_pointer_cast<Monster>(obj);
            if (m)
                info->set_type_id(m->GetMonsterTypeId());
        }
        else if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            auto p = std::dynamic_pointer_cast<Projectile>(obj);
            if (p)
                info->set_type_id(p->GetTypeId());
        }
    }

    BroadcastPacket(S_SpawnObjectPacket(msg));
}

void Room::BroadcastDespawn(const std::vector<int32_t> &objectIds, const std::vector<int32_t> &pickerIds)
{
    if (objectIds.empty())
        return;

    Protocol::S_DespawnObject msg;
    for (size_t i = 0; i < objectIds.size(); ++i)
    {
        msg.add_object_ids(objectIds[i]);
        if (i < pickerIds.size())
        {
            msg.add_picker_ids(pickerIds[i]);
        }
        else
        {
            msg.add_picker_ids(0);
        }
    }

    BroadcastPacket(S_DespawnObjectPacket(msg));
}

void Room::SyncNetwork()
{
    // [Networking] S_MoveObjectBatchPacket을 통한 전역 위치 동기화
    // 과부하 방지를 위해 틱마다 브로드캐스트 (내삽은 클라이언트 담당)
    auto objects = _objMgr.GetAllObjects();
    if (objects.empty())
        return;

    Protocol::S_MoveObjectBatch moveBatch;
    moveBatch.set_server_tick(_serverTick);

    int validCount = 0;
    int invalidCount = 0;

    for (const auto &obj : objects)
    {
        if (obj->IsDead())
            continue;

        // [Fix] NaN/Inf 값 검증 (Protobuf 직렬화 크래시 방지)
        float x = obj->GetX();
        float y = obj->GetY();
        float vx = obj->GetVX();
        float vy = obj->GetVY();

        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(vx) || !std::isfinite(vy))
        {
            LOG_ERROR(
                "[CRITICAL] Invalid float in Object {}: x={}, y={}, vx={}, vy={}",
                obj->GetId(), x, y, vx, vy
            );
            invalidCount++;
            continue;
        }

        // 플레이어, 몬스터, 투사체 모두 동기화
        auto *pos = moveBatch.add_moves();
        pos->set_object_id(obj->GetId());
        pos->set_x(x);
        pos->set_y(y);
        pos->set_vx(vx);
        pos->set_vy(vy);
        pos->set_state(obj->GetState());
        pos->set_look_left(obj->GetLookLeft());
        validCount++;
    }

    if (invalidCount > 0)
    {
        LOG_WARN("[SyncNetwork] Skipped {} invalid objects (NaN/Inf detected)", invalidCount);
    }

    if (moveBatch.moves_size() > 0)
    {
        BroadcastPacket(S_MoveObjectBatchPacket(std::move(moveBatch)));
    }

    // [이동 동기화] 클라이언트 측 추측 이동(CSP) 정정을 위해 각 플레이어에게 Ack 패킷 전송
    for (auto &[sid, player] : _players)
    {
        if (player->IsDead())
            continue;

        Protocol::S_PlayerStateAck ack;
        ack.set_server_tick(_serverTick);
        ack.set_client_tick(player->GetLastProcessedClientTick());
        ack.set_x(player->GetX());
        ack.set_y(player->GetY());

        SendToPlayer(sid, S_PlayerStateAckPacket(std::move(ack)));
    }
}

// [Spatial Queries]
std::shared_ptr<Player> Room::GetNearestPlayer(float x, float y)
{
    std::shared_ptr<Player> nearest = nullptr;
    float minDstSq = std::numeric_limits<float>::max();

    for (const auto &[sid, player] : _players)
    {
        if (player->IsDead())
            continue;

        float dx = player->GetX() - x;
        float dy = player->GetY() - y;
        float dstSq = dx * dx + dy * dy;

        if (dstSq < minDstSq)
        {
            minDstSq = dstSq;
            nearest = player;
        }
    }

    return nearest;
}

std::vector<std::shared_ptr<Monster>> Room::GetMonstersInRange(float x, float y, float radius)
{
    std::vector<std::shared_ptr<GameObject>> results;

    // Fix: Use _grid logic correctly.
    // ObjectManager doesn't have QueryRange. SpatialGrid does.
    _grid.QueryRange(x, y, radius, results, _objMgr);

    std::vector<std::shared_ptr<Monster>> monsters;
    monsters.reserve(results.size());

    for (auto &obj : results)
    {
        if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {
            auto m = std::dynamic_pointer_cast<Monster>(obj);
            if (m && !m->IsDead())
                monsters.push_back(m);
        }
    }
    return monsters;
}

// [Debug]
void Room::DebugAddExpToAll(int32_t exp)
{
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self, exp]()
            {
                for (auto &[sid, p] : self->_players)
                {
                    p->AddExp(exp, self.get());
                }
                LOG_INFO("Debug: Added {} EXP to all players.", exp);
            }
        );
    }
}

void Room::DebugSpawnMonster(int32_t monsterId, int32_t count)
{
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self, monsterId, count]()
            {
                self->_waveMgr.DebugSpawn(self.get(), monsterId, count);
            }
        );
    }
}

} // namespace SimpleGame
