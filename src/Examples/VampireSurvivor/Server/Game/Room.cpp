#include "Game/Room.h"
#include "Core/UserDB.h"
#include "Entity/Monster.h"
#include "Entity/Projectile.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/Packet/IPacket.h"
#include "System/Packet/PacketBroadcast.h"
#include "System/Thread/IStrand.h"
#include <limits>

namespace SimpleGame {

Room::Room(
    int roomId, std::shared_ptr<System::ITimer> timer, std::shared_ptr<System::IStrand> strand,
    std::shared_ptr<UserDB> userDB
)
    : _roomId(roomId), _timer(timer), _strand(strand), _waveMgr(_objMgr, _grid, roomId), _userDB(userDB)
{
}

Room::~Room()
{
    Stop();
}

void Room::Start()
{
    std::cout << "[DEBUG] Room::Start(" << _roomId << ")" << std::endl;
    if (_timer)
    {
        // 50ms = 20 FPS
        _timerHandle = _timer->SetInterval(1, 50, this);
        std::cout << "[DEBUG] Room " << _roomId << " Game Loop Started (20 FPS). TimerHandle: " << _timerHandle
                  << std::endl;
    }
    else
    {
        std::cerr << "[ERROR] Room " << _roomId << " has NO TIMER!" << std::endl;
    }
    // WaveManager will be started when first player enters
    LOG_INFO("Room {} created. Waiting for players...", _roomId);
}

void Room::Stop()
{
    if (_timer && _timerHandle)
    {
        _timer->CancelTimer(_timerHandle);
        _timerHandle = 0;
    }
}

void Room::StartGame()
{
    if (_gameStarted)
        return; // Already started

    _gameStarted = true;
    _waveMgr.Start();
    LOG_INFO("Game started in Room {}! Wave spawning begins.", _roomId);
}

void Room::Reset()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // 1. Clear all objects (monsters, projectiles, etc.) except players
    auto allObjects = _objMgr.GetAllObjects();
    for (const auto &obj : allObjects)
    {
        if (obj->GetType() != Protocol::ObjectType::PLAYER)
        {
            _grid.Remove(obj);
            _objMgr.RemoveObject(obj->GetId());
        }
    }

    // 2. Reset WaveManager
    _waveMgr.Reset();

    // 3. Reset game state
    _gameStarted = false;
    _totalRunTime = 0.0f;

    LOG_INFO("Room {} has been reset. Ready for new game.", _roomId);
}

void Room::OnTimer(uint32_t timerId, void *pParam)
{
    // Offload to Strand (Worker Thread)
    // capture shared_ptr to keep room alive if needed, but 'this' is safe if Timer cancels on destruct.
    // However, safest pattern is capturing shared_from_this() if we want to be 100% sure,
    // but OnTimer is called by Timer which we manage.
    // Let's use 'this' for now as Timer is cancelled in Stop/Destructor.

    if (_strand)
    {
        _strand->Post(
            [this]()
            {
                // Fixed Delta Time = 0.05f
                this->Update(0.05f);
            }
        );
    }
    else
    {
        // Fallback or Error
        Update(0.05f);
    }
}

// Helper to broadcast Protobuf message
// Helper to broadcast Protobuf message - Overloaded for specific packets
void BroadcastProto(Room *room, const Protocol::S_SpawnObject &msg)
{
    S_SpawnObjectPacket packet(msg);
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, const Protocol::S_DespawnObject &msg)
{
    S_DespawnObjectPacket packet(msg);
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, const Protocol::S_MoveObjectBatch &msg)
{
    S_MoveObjectBatchPacket packet(msg);
    room->BroadcastPacket(packet);
}

void Room::BroadcastSpawn(const std::vector<std::shared_ptr<GameObject>> &objects)
{
    if (objects.empty())
        return;

    Protocol::S_SpawnObject msg;
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
        // TypeId customization could be added here if needed
    }
    BroadcastProto(this, msg);
}

void Room::BroadcastDespawn(const std::vector<int32_t> &objectIds)
{
    if (objectIds.empty())
        return;

    Protocol::S_DespawnObject msg;
    for (int32_t id : objectIds)
    {
        msg.add_object_ids(id);
    }
    BroadcastProto(this, msg);
}

void Room::Enter(std::shared_ptr<Player> player)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _players[player->GetSessionId()] = player;
    player->SetRoomId(_roomId);

    // Load Persistent Data
    // TODO: Need UserId properly. Assuming SessionId maps to UserId for now or passed separately.
    // In real app, Player object should store UserId from Login.
    // For now, cast SessionId to int (Warning: data loss if big)
    int userId = (int)player->GetSessionId();
    if (_userDB)
    {
        auto skills = _userDB->GetUserSkills(userId);
        player->ApplySkills(skills);
        LOG_INFO("Applied {} skills to Player {}", skills.size(), userId);
    }

    // Add to Game Systems
    _objMgr.AddObject(player);
    _grid.Add(player);

    LOG_INFO("Player {} entered Room {}. Total Players: {}", player->GetSessionId(), _roomId, _players.size());

    if (_players.size() == 1 && !_gameStarted)
    {
        StartGame();
    }

    LOG_INFO("Player {} entered Room {}. Waiting for C_GAME_READY.", player->GetSessionId(), _roomId);
}

void Room::OnPlayerReady(uint64_t sessionId)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    auto it = _players.find(sessionId);
    if (it == _players.end())
    {
        LOG_WARN("OnPlayerReady: Player {} not found in room {}", sessionId, _roomId);
        return;
    }

    auto player = it->second;
    LOG_INFO("Player {} is ready in Room {}", sessionId, _roomId);

    // ===== Send Initial State to New Player =====
    // 1. Send all existing objects in the room to the newly joined player
    auto allObjects = _objMgr.GetAllObjects();
    if (!allObjects.empty())
    {
        Protocol::S_SpawnObject existingObjects;
        for (const auto &obj : allObjects)
        {
            // Skip self (the new player was just added, but client already knows about itself)
            if (obj->GetId() == player->GetId())
                continue;

            auto *info = existingObjects.add_objects();
            info->set_object_id(obj->GetId());
            info->set_type(obj->GetType());
            info->set_x(obj->GetX());
            info->set_y(obj->GetY());
            info->set_hp(obj->GetHp());
            info->set_max_hp(obj->GetMaxHp());
            info->set_state(obj->GetState());

            // Set type_id for monsters
            if (obj->GetType() == Protocol::ObjectType::MONSTER)
            {
                auto monster = std::dynamic_pointer_cast<Monster>(obj);
                if (monster)
                {
                    info->set_type_id(monster->GetMonsterTypeId());
                }
            }
        }

        // Send to the new player only
        if (existingObjects.objects_size() > 0)
        {
            S_SpawnObjectPacket packet(existingObjects);
            player->GetSession()->SendPacket(packet);
            LOG_INFO("Sent {} existing objects to ready player {}", existingObjects.objects_size(), sessionId);
        }
    }

    // 2. Broadcast the new player's spawn to all other players
    Protocol::S_SpawnObject newPlayerSpawn;
    auto *info = newPlayerSpawn.add_objects();
    info->set_object_id(player->GetId());
    info->set_type(Protocol::ObjectType::PLAYER);
    info->set_x(player->GetX());
    info->set_y(player->GetY());
    info->set_hp(player->GetHp());
    info->set_max_hp(player->GetMaxHp());
    info->set_state(Protocol::ObjectState::IDLE);

    BroadcastProto(this, newPlayerSpawn);
    LOG_INFO("Broadcasted ready player {} spawn to all players in room", sessionId);
}

void Room::Leave(uint64_t sessionId)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    auto it = _players.find(sessionId);
    if (it != _players.end())
    {
        auto player = it->second;
        player->SetRoomId(0);

        // Remove from Systems
        _grid.Remove(player);
        _objMgr.RemoveObject(player->GetId());

        // Broadcast Despawn
        Protocol::S_DespawnObject despawn;
        despawn.add_object_ids(player->GetId());
        BroadcastProto(this, despawn);

        _players.erase(it);
        LOG_INFO("Player {} left Room {}", sessionId, _roomId);

        // Reset room when all players leave
        if (_players.empty())
        {
            Stop();
            LOG_INFO("Room {} is empty. Stopping game loop timer.", _roomId);

            // Reset room state for next game
            Reset();
        }
    }
}

void Room::Update(float deltaTime)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // 0. Wave & Spawning Update
    // LOG_DEBUG("Room {} Update Tick", _roomId); // Too spammy? Maybe only for Room 1
    _waveMgr.Update(deltaTime, this);
    _totalRunTime += deltaTime;

    // 1. Physics & Logic Update
    auto objects = _objMgr.GetAllObjects();
    Protocol::S_MoveObjectBatch moveBatch;
    size_t currentBatchSize = 0;
    const size_t MAX_PAYLOAD_BYTES = 10000;
    const size_t PACKET_OVERHEAD = 64;

    for (auto &obj : objects)
    {
        // AI / Logic Update
        obj->Update(deltaTime, this);

        float oldX = obj->GetX();
        float oldY = obj->GetY();

        // Simple Euler Integration
        // TODO: Server-side validation of input?
        // For now, assume Velocity is set by Input Packet (C_MOVE)
        if (obj->GetVX() != 0 || obj->GetVY() != 0)
        {
            float newX = oldX + obj->GetVX() * deltaTime;
            float newY = oldY + obj->GetVY() * deltaTime;

            // Map Bounds Check (TODO)

            obj->SetPos(newX, newY);
            _grid.Update(obj, oldX, oldY);
        }

        // 2. Network Optimization (Delta Sync)
        bool isDirty = false;

        // A. State Changed (Idle <-> Moving)
        if (obj->GetState() != obj->GetLastSentState())
            isDirty = true;

        // B. Velocity Changed (Threshold)
        float dvx = obj->GetVX() - obj->GetLastSentVX();
        float dvy = obj->GetVY() - obj->GetLastSentVY();
        if ((dvx * dvx + dvy * dvy) > 0.01f)
            isDirty = true; // > 0.1 difference

        // C. Hard Sync (Drift Correction) - Every 1.0s
        if (_totalRunTime - obj->GetLastSentTime() > 1.0f)
            isDirty = true;

        if (isDirty)
        {
            // Update Sync State
            obj->UpdateLastSentState(_totalRunTime);

            // Create Proto Message (temp to check size)
            Protocol::ObjectPos move; // Create temporary to check size
            move.set_object_id(obj->GetId());
            move.set_x(obj->GetX());
            move.set_y(obj->GetY());
            move.set_vx(obj->GetVX());
            move.set_vy(obj->GetVY());

            size_t msgSize = move.ByteSizeLong();

            // Packet Splitting Check
            if (currentBatchSize + msgSize > MAX_PAYLOAD_BYTES - PACKET_OVERHEAD)
            {
                BroadcastProto(this, moveBatch);
                moveBatch.Clear();
                currentBatchSize = 0;
            }

            // Add to Batch
            auto *added = moveBatch.add_moves();
            *added = move; // Copy
            currentBatchSize += msgSize;
        }
    }

    // 2. Broadcast Batched Movement (Remaining)
    if (moveBatch.moves_size() > 0)
    {
        BroadcastProto(this, moveBatch);
    }

    // 3. Collision Detection & Combat Logic
    std::vector<int32_t> deadObjectIds;
    Protocol::S_DamageEffect damageEffect;

    for (auto &obj : objects)
    {
        if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            auto proj = std::static_pointer_cast<Projectile>(obj);
            if (proj->IsExpired())
                continue;

            // Query nearby objects (Projectiles usually have small radius, e.g. 0.5m)
            auto nearby = _grid.QueryRange(proj->GetX(), proj->GetY(), proj->GetRadius() + 0.5f);
            for (auto &target : nearby)
            {
                if (target->GetId() == proj->GetId())
                    continue;
                if (target->GetId() == proj->GetOwnerId())
                    continue; // Don't hit owner

                if (target->GetType() == Protocol::ObjectType::MONSTER)
                {
                    auto monster = std::static_pointer_cast<Monster>(target);
                    if (monster->IsDead())
                        continue;

                    // Simple Circular Collision
                    float dx = proj->GetX() - monster->GetX();
                    float dy = proj->GetY() - monster->GetY();
                    float distSq = dx * dx + dy * dy;
                    float sumRad = proj->GetRadius() + monster->GetRadius();

                    if (distSq <= sumRad * sumRad)
                    {
                        // HIT!
                        monster->TakeDamage(proj->GetDamage(), this);
                        proj->SetHit();

                        // Record effect
                        damageEffect.add_target_ids(monster->GetId());
                        damageEffect.add_damage_values(proj->GetDamage());

                        if (monster->IsDead())
                        {
                            deadObjectIds.push_back(monster->GetId());
                        }
                        break; // Projectile hit one target and disappeared
                    }
                }
            }
        }
    }

    // 4. Post-Update: Cleanup & Broadcast Effects
    if (damageEffect.target_ids_size() > 0)
    {
        S_DamageEffectPacket damagePkt(damageEffect);
        BroadcastPacket(damagePkt);
    }

    // Cleanup expired/dead objects
    std::vector<int32_t> despawnIds;
    for (auto &obj : objects)
    {
        bool shouldRemove = false;
        if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            if (std::static_pointer_cast<Projectile>(obj)->IsExpired())
                shouldRemove = true;
        }
        else if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {
            if (std::static_pointer_cast<Monster>(obj)->IsDead())
                shouldRemove = true;
        }

        if (shouldRemove)
        {
            despawnIds.push_back(obj->GetId());
            _grid.Remove(obj);
            _objMgr.RemoveObject(obj->GetId());
        }
    }

    if (!despawnIds.empty())
    {
        BroadcastDespawn(despawnIds);
    }
}

std::shared_ptr<Player> Room::GetNearestPlayer(float x, float y)
{
    std::shared_ptr<Player> nearest = nullptr;
    float minDistSq = std::numeric_limits<float>::max();

    for (auto &pair : _players)
    {
        auto p = pair.second;
        float dx = p->GetX() - x;
        float dy = p->GetY() - y;
        float dSq = dx * dx + dy * dy;
        if (dSq < minDistSq)
        {
            minDistSq = dSq;
            nearest = p;
        }
    }
    return nearest;
}

// [Phase 3] Broadcast Optimization
void Room::BroadcastPacket(const System::IPacket &pkt)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // Collect real sessions for optimized broadcast
    std::vector<System::Session *> realSessions;
    realSessions.reserve(_players.size());

    for (auto &pair : _players)
    {
        auto *isess = pair.second->GetSession();
        if (!isess)
            continue;

        // Dynamic cast to check if it's a real Session (vs MockSession)
        // This overhead is acceptable compared to serialization savings for many players.
        auto *sess = dynamic_cast<System::Session *>(isess);
        if (sess)
        {
            realSessions.push_back(sess);
        }
        else
        {
            // Fallback for MockSession (e.g. Unit Tests)
            isess->SendPacket(pkt);
        }
    }

    if (!realSessions.empty())
    {
        System::PacketBroadcast::Broadcast(pkt, realSessions);
    }
}

size_t Room::GetPlayerCount()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _players.size();
}

} // namespace SimpleGame
