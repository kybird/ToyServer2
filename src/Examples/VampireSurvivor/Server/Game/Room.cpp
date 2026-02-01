#include "Game/Room.h"
#include "Core/UserDB.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Game/CombatManager.h"
#include "Game/DamageEmitter.h"
#include "Game/GameConfig.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Math/Vector2.h"
#include "Protocol/game.pb.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Packet/IPacket.h"
#include "System/Packet/PacketBroadcast.h"
#include "System/Packet/PacketBuilder.h" // [New]
#include "System/Packet/PacketPtr.h"     // [New]
#include "System/Thread/IStrand.h"
#include <algorithm>
#include <chrono>
#include <limits>

namespace SimpleGame {

Room::Room(
    int roomId, std::shared_ptr<System::IDispatcher> dispatcher, std::shared_ptr<System::ITimer> timer,
    std::shared_ptr<System::IStrand> strand, std::shared_ptr<UserDB> userDB
)
    : _roomId(roomId), _timer(std::move(timer)), _strand(std::move(strand)), _waveMgr(_objMgr, _grid, roomId),
      _dispatcher(std::move(dispatcher)), _userDB(std::move(userDB))
{
    _combatMgr = std::make_unique<CombatManager>();
    _effectMgr = std::make_unique<EffectManager>();
}

Room::~Room()
{
    Stop();
}

void Room::Start()
{
    std::cout << "[DEBUG] Room::Start(" << _roomId << ")\n";
    if (_timer != nullptr)
    {
        // Use constants for TPS and interval
        _timerHandle = _timer->SetInterval(1, GameConfig::TICK_INTERVAL_MS, this);
        std::cout << "[DEBUG] Room " << _roomId << " Game Loop Started (" << GameConfig::TPS << " TPS, "
                  << GameConfig::TICK_INTERVAL_MS << "ms). TimerHandle: " << _timerHandle << "\n";
    }
    else
    {
        std::cerr << "[ERROR] Room " << _roomId << " has NO TIMER!\n";
    }
    // WaveManager will be started when first player enters
    LOG_INFO("Room {} created. Waiting for players...", _roomId);
}

void Room::Stop()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    LOG_INFO("Room {} STOP initiated. (Players: {})", _roomId, _players.size());

    if (_timer != nullptr && _timerHandle != 0)
    {
        _timer->CancelTimer(_timerHandle);
        _timerHandle = 0;
    }

    // Clear players and notify them if necessary
    // During server shutdown, we just clear the map to release shared_ptrs
    _players.clear();
    _objMgr.Clear();

    LOG_INFO("Room {} STOP finished.", _roomId);
}

void Room::StartGame()
{
    if (_gameStarted)
        return; // Already started

    _gameStarted = true;

    // Restart Timer if it was stopped
    if (_timerHandle == 0)
    {
        Start();
    }

    _waveMgr.Start();
    LOG_INFO("Game started in Room {}! Wave spawning begins.", _roomId);
}

void Room::Reset()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // 1. Clear all objects (monsters, projectiles, etc.) except players
    // 1. Clear all objects (monsters, projectiles, etc.) except players
    // 1. Clear ALL objects including stale players
    _objMgr.Clear();

    // Force clear Grid and release memory
    _grid.HardClear();

    // 2. Reset WaveManager
    _waveMgr.Reset();

    // 3. Reset game state
    _gameStarted = false;
    _isGameOver = false;
    _totalRunTime = 0.0f;
    _serverTick = 0;

    // 4. Reset Performance Tracking
    _lastPerfLogTime = 0.0f;
    _totalUpdateSec = 0.0f;
    _updateCount = 0;
    _maxUpdateSec = 0.0f;
    _physicsTimeSec = 0.0f;
    _networkTimeSec = 0.0f;
    _overlapTimeSec = 0.0f;
    _combatTimeSec = 0.0f;

    LOG_INFO("Room {} has been reset. Ready for new game.", _roomId);
}

void Room::OnTimer(uint32_t timerId, void *pParam)
{
    (void)timerId;
    (void)pParam;
    // Offload to Strand (Worker Thread)
    // capture shared_ptr to keep room alive if needed, but 'this' is safe if Timer cancels on destruct.
    // However, safest pattern is capturing shared_from_this() if we want to be 100% sure,
    // but OnTimer is called by Timer which we manage.
    // Let's use 'this' for now as Timer is cancelled in Stop/Destructor.
    _serverTick++;

    // LOG_DEBUG("[ServerTick] {}", _serverTick);
    if (_strand != nullptr)
    {
        _strand->Post(
            [this]()
            {
                // Use constant deltaTime

                this->Update(GameConfig::TICK_INTERVAL_SEC);
            }
        );
    }
    else
    {
        // Fallback or Error
        Update(GameConfig::TICK_INTERVAL_SEC);
    }
}

// (Redundant BroadcastProto helpers removed)

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

        // Set type_id for monsters and projectiles
        if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {
            auto monster = std::dynamic_pointer_cast<Monster>(obj);
            if (monster != nullptr)
                info->set_type_id(monster->GetMonsterTypeId());
        }
        else if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            auto proj = std::dynamic_pointer_cast<Projectile>(obj);
            if (proj != nullptr)
                info->set_type_id(proj->GetTypeId());
        }
    }
    BroadcastPacket(S_SpawnObjectPacket(std::move(msg)));
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
            msg.add_picker_ids(pickerIds[i]);
        else
            msg.add_picker_ids(0);
    }
    BroadcastPacket(S_DespawnObjectPacket(std::move(msg)));
}

void Room::Enter(const std::shared_ptr<Player> &player)
{
    // [Safety] 과부하로 인한 Disconnect 지연 시, 재접속하면 기존 좀비 세션이 남아있을 수 있음.
    // 여기서는 테스트 편의를 위해(혹은 1인 방 가정) 기존 플레이어를 정리함.
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (!_players.empty())
        {
            LOG_WARN("Room::Enter - Clearing {} stale players for new connection.", _players.size());
            for (auto &pair : _players)
            {
                // ObjectManager에서 제거해야 Rebuild 시 포함되지 않음
                _objMgr.RemoveObject(pair.second->GetId());
            }
            _players.clear();
        }
    }

    // [Async Loading Flow]
    // 1. Register Session in Room (So networking works)
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _players[player->GetSessionId()] = player;
        player->SetRoomId(_roomId);
        // Do NOT add to _objMgr or _grid yet. Player is "Loading".
    }

    LOG_INFO("Player {} connecting to Room {}. Loading Data...", player->GetSessionId(), _roomId);

    // 2. Async Load Data
    // We must keep player alive.
    // We assume _userDB is valid.
    int32_t userId = static_cast<int32_t>(player->GetSessionId()); // TODO: Use real UserId
    auto self = shared_from_this();                                // Keep Room alive during DB call

    if (_userDB != nullptr)
    {
        _userDB->GetUserSkills(
            userId,
            [self, player](const std::vector<std::pair<int, int>> &skills)
            {
                // [Main Thread] Callback
                // 3. Finalize Entry
                std::lock_guard<std::recursive_mutex> lock(self->_mutex);

                // Check if player is still in the room (might have disconnected during load)
                auto it = self->_players.find(player->GetSessionId());
                if (it == self->_players.end() || it->second != player)
                {
                    LOG_WARN("Player {} disconnected while loading.", player->GetSessionId());
                    return;
                }

                // Apply Data
                player->ApplySkills(skills);
                LOG_INFO("Applied {} skills to Player {}", skills.size(), player->GetSessionId());

                // Add to Game Systems (Now visible)
                self->_objMgr.AddObject(player);
                self->_grid.Add(player);

                LOG_INFO(
                    "Player {} entered Room {}. Total Players: {}",
                    player->GetSessionId(),
                    self->_roomId,
                    self->_players.size()
                );
                LOG_INFO("Player {} entered Room {}. Waiting for C_GAME_READY.", player->GetSessionId(), self->_roomId);

                // [Refactor] Apply Default Skills from PlayerTemplate
                const auto *pTmpl =
                    DataManager::Instance().GetPlayerTemplate(1); // Assuming type ID 1 for now (TODO: Use real type ID)
                if (pTmpl != nullptr && !pTmpl->defaultSkills.empty())
                {
                    player->AddDefaultSkills(pTmpl->defaultSkills);
                    LOG_INFO(
                        "Applied {} default skills to Player {}", pTmpl->defaultSkills.size(), player->GetSessionId()
                    );
                }
            }
        );
    }
    else
    {
        // No DB? Just enter immediately
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        // [Refactor] Apply Default Skills from PlayerTemplate
        const auto *pTmpl = DataManager::Instance().GetPlayerTemplate(1);
        if (pTmpl != nullptr && !pTmpl->defaultSkills.empty())
        {
            player->AddDefaultSkills(pTmpl->defaultSkills);
        }

        _objMgr.AddObject(player);
        _grid.Add(player);
        LOG_INFO("Player {} entered Room {} (No DB).", player->GetSessionId(), _roomId);
    }
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
    player->SetReady(true);
    LOG_INFO("Player {} is ready in Room {}", sessionId, _roomId);

    // [New] Start game loop only when at least one player is fully ready
    if (!_gameStarted)
    {
        StartGame();
    }

    // ===== Send Initial State to New Player =====
    // 1. Send all existing objects in the room to the newly joined player
    auto allObjects = _objMgr.GetAllObjects();
    if (!allObjects.empty())
    {
        Protocol::S_SpawnObject existingObjects;
        existingObjects.set_server_tick(_serverTick); // Anchor for Tick Sync
        for (const auto &obj : allObjects)
        {
            // [Fix] Include self to ensure client spawns its own character.
            // Client relies on S_SpawnObject to create the local player object.

            auto *info = existingObjects.add_objects();
            info->set_object_id(obj->GetId());
            info->set_type(obj->GetType());
            info->set_x(obj->GetX());
            info->set_y(obj->GetY());
            info->set_hp(obj->GetHp());
            info->set_max_hp(obj->GetMaxHp());
            info->set_state(obj->GetState());
            info->set_vx(obj->GetVX());
            info->set_vy(obj->GetVY());

            // Set type_id for monsters and projectiles
            if (obj->GetType() == Protocol::ObjectType::MONSTER)
            {
                auto monster = std::dynamic_pointer_cast<Monster>(obj);
                if (monster != nullptr)
                {
                    info->set_type_id(monster->GetMonsterTypeId());
                }
            }
            else if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
            {
                auto proj = std::dynamic_pointer_cast<Projectile>(obj);
                if (proj != nullptr)
                {
                    info->set_type_id(proj->GetTypeId());
                }
            }
        }

        // Send to the new player only
        if (existingObjects.objects_size() > 0)
        {
            S_SpawnObjectPacket packet(existingObjects);
            SendToPlayer(sessionId, packet);
            LOG_INFO("Sent {} existing objects to ready player {}", existingObjects.objects_size(), sessionId);
        }
    }

    // 2. Broadcast the new player's spawn to all other players
    Protocol::S_SpawnObject newPlayerSpawn;
    newPlayerSpawn.set_server_tick(_serverTick); // [Fix] Anchor Tick set
    auto *info = newPlayerSpawn.add_objects();
    info->set_object_id(player->GetId());
    info->set_type(Protocol::ObjectType::PLAYER);
    info->set_x(player->GetX());
    info->set_y(player->GetY());
    info->set_hp(player->GetHp());
    info->set_max_hp(player->GetMaxHp());
    info->set_state(Protocol::ObjectState::IDLE);

    BroadcastPacket(S_SpawnObjectPacket(std::move(newPlayerSpawn)), sessionId);
    LOG_INFO("Broadcasted ready player {} spawn to other players in room", sessionId);
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
        BroadcastPacket(S_DespawnObjectPacket(std::move(despawn)));

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
    auto startTime = std::chrono::steady_clock::now();
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // [Stage 0 Verification] Broadcast S_DEBUG_SERVER_TICK every 1s (Real Time Accumulator)
    // _debugBroadcastTimer += deltaTime;
    // if (_debugBroadcastTimer >= 1.0f)
    // {
    //     _debugBroadcastTimer -= 1.0f;

    //     Protocol::S_DebugServerTick dbgMsg;
    //     dbgMsg.set_server_tick(_serverTick);
    //     S_DebugServerTickPacket dbgPkt(dbgMsg);
    //     BroadcastPacket(dbgPkt);

    //     LOG_INFO("[DebugTick] serverTick = {}", _serverTick);
    // }

    if (!_isGameOver)
    {
        _waveMgr.Update(deltaTime, this);
    }
    _totalRunTime += deltaTime;

    // 1. Physics & Logic Update
    auto t0 = std::chrono::steady_clock::now();

    // [Optimization] Fetch objects after spawning new ones to include them in this tick's movement/physics
    auto objects = _objMgr.GetAllObjects();

    // Separate updates for batching
    std::vector<Protocol::ObjectPos> monsterMoves;
    std::vector<Protocol::ObjectPos> playerMoves;

    for (auto &obj : objects)
    {
        UpdateObject(obj, deltaTime, monsterMoves, playerMoves);
    }
    auto t1 = std::chrono::steady_clock::now();

    // === 2. Broadcast Monsters (Reactive Sync) ===
    // 5 TPS = Every 5 ticks (0.2s interval)
    bool isSyncTick = (_serverTick % 5 == 0);
    std::vector<Protocol::ObjectPos> priorityMoves;
    std::vector<Protocol::ObjectPos> periodicMoves;

    for (const auto &move : monsterMoves)
    {
        auto obj = _objMgr.GetObject(move.object_id());
        if (!obj)
            continue;

        // Detect significant velocity change (especially STOPPING)
        bool wasMoving = (std::abs(obj->GetLastSentVX()) > 0.01f || std::abs(obj->GetLastSentVY()) > 0.01f);
        bool isMoving = (std::abs(move.vx()) > 0.01f || std::abs(move.vy()) > 0.01f);

        if (wasMoving != isMoving)
        {
            priorityMoves.push_back(move);
        }
        else if (isSyncTick)
        {
            periodicMoves.push_back(move);
        }
    }

    // Combine and send in batches
    std::vector<Protocol::ObjectPos> allMovesToSync = std::move(priorityMoves);
    allMovesToSync.insert(allMovesToSync.end(), periodicMoves.begin(), periodicMoves.end());

    if (!allMovesToSync.empty())
    {
        const size_t BATCH_SIZE = 50;
        size_t totalMoves = allMovesToSync.size();
        size_t processed = 0;

        while (processed < totalMoves)
        {
            Protocol::S_MoveObjectBatch batch;
            batch.set_server_tick(_serverTick);

            size_t count = std::min(BATCH_SIZE, totalMoves - processed);
            for (size_t i = 0; i < count; ++i)
            {
                auto &m = allMovesToSync[processed + i];
                *batch.add_moves() = m;

                // Update last sent state to track changes
                auto obj = _objMgr.GetObject(m.object_id());
                if (obj)
                    obj->UpdateLastSentState(_totalRunTime, _serverTick);
            }

            BroadcastPacket(S_MoveObjectBatchPacket(std::move(batch)));
            processed += count;
        }
    }

    // === 3. Broadcast Players (filter self) ===
    BroadcastPlayerMoves(playerMoves);

    // === 4. Send Player Ack (authoritative snapshot) ===
    SendPlayerAcks();
    auto t2 = std::chrono::steady_clock::now();

    // === [NEW] 효과 업데이트 (Physics 후, Combat 전) ===
    _effectMgr->Update(_totalRunTime, this);

    // === 5. Overlap Resolution (Cell-Centric Optimization) ===
    auto t3 = std::chrono::steady_clock::now();
    _grid.Rebuild(objects);

    // [DEBUG] Check grid compilation
    /*
    static int debugLogTick = 0;
    if (++debugLogTick % 250 == 0) // 매 10초마다 (25TPS 기준)
    {
        int totalMonstersInGrid = 0;
        for(int i=0; i<SpatialGrid::TOTAL_CELLS; ++i) {
            totalMonstersInGrid += _grid.GetMonsterIds(i).size();
        }
        LOG_INFO("[DEBUG] Monsters in Grid: {} / Total Objects: {}", totalMonstersInGrid, objects.size());
    }
    */

    for (int cellIdx = 0; cellIdx < SpatialGrid::TOTAL_CELLS; ++cellIdx)
    {
        const auto &cellIds = _grid.GetMonsterIds(cellIdx);
        if (cellIds.empty())
            continue;

        std::array<int, 9> neighbors;
        _grid.GetNeighborCells(cellIdx, neighbors);

        for (int32_t myId : cellIds)
        {
            auto obj = _objMgr.GetObject(myId);
            if (!obj)
                continue;

            float myX = obj->GetX();
            float myY = obj->GetY();
            float myRadius = obj->GetRadius();

            for (int nIdx : neighbors)
            {
                const auto &otherIds = _grid.GetMonsterIds(nIdx);

                for (int32_t otherId : otherIds)
                {
                    // 중복 방지: 자신보다 큰 ID만 체크하거나, 셀 인덱스 순서 활용 가능
                    // 여기서는 단순히 ID 비교로 중복 회피
                    if (myId >= otherId)
                        continue;

                    auto other = _objMgr.GetObject(otherId);
                    if (!other)
                        continue;

                    float dx = myX - other->GetX();
                    float dy = myY - other->GetY();
                    float distSq = dx * dx + dy * dy;

                    float minDist = myRadius + other->GetRadius() + 0.1f;
                    float minDistSq = minDist * minDist;

                    if (distSq < minDistSq && distSq > 0.0001f)
                    {
                        float dist = std::sqrt(distSq);
                        float invDist = 1.0f / dist;
                        float overlap = minDist - dist;

                        // 밀어내기 강도 조절 (Cell-centric에서는 한 번에 처리하므로 약간 더 강하게)
                        float pushX = (dx * invDist) * overlap * 0.5f;
                        float pushY = (dy * invDist) * overlap * 0.5f;

                        obj->SetPos(obj->GetX() + pushX, obj->GetY() + pushY);
                        other->SetPos(other->GetX() - pushX, other->GetY() - pushY);

                        // Note: Grid rebuild를 매번 하지 않으므로 이번 틱의 다음 계산에서는
                        // 약간 오차가 있을 수 있으나, 25Hz에서는 무시 가능한 수준입니다.
                    }
                }
            }
        }
    }

    // 3. Combat & Collision Resolution (Refactored)
    auto t4 = std::chrono::steady_clock::now();
    _combatMgr->Update(deltaTime, this);
    auto t5 = std::chrono::steady_clock::now();

    // Accumulate detailed times
    _physicsTimeSec += std::chrono::duration<float>(t1 - t0).count();
    _networkTimeSec += std::chrono::duration<float>(t2 - t1).count();
    _overlapTimeSec += std::chrono::duration<float>(t4 - t3).count();
    _combatTimeSec += std::chrono::duration<float>(t5 - t4).count();

    // --- Performance Tracking ---
    auto endTime = std::chrono::steady_clock::now();
    float elapsedSec = std::chrono::duration<float>(endTime - startTime).count();
    _totalUpdateSec += elapsedSec;
    _updateCount++;
    _maxUpdateSec = std::max(_maxUpdateSec, elapsedSec);

    if (_totalRunTime - _lastPerfLogTime >= 5.0f)
    {
        float total = _updateCount > 0 ? _updateCount : 1;
        float avgMs = (_totalUpdateSec / total) * 1000.0f;
        float physMs = (_physicsTimeSec / total) * 1000.0f;
        float netMs = (_networkTimeSec / total) * 1000.0f;
        float overMs = (_overlapTimeSec / total) * 1000.0f;
        float combatMs = (_combatTimeSec / total) * 1000.0f;

        int monsterCount = static_cast<int>(_objMgr.GetAliveMonsterCount());

        LOG_INFO(
            "[Performance] Room:{} | Monsters:{} | Total:{:.2f}ms (Phys:{:.2f}, Net:{:.2f}, Overlap:{:.2f}, "
            "Combat:{:.2f})",
            _roomId,
            monsterCount,
            avgMs,
            physMs,
            netMs,
            overMs,
            combatMs
        );

        _totalUpdateSec = 0.0f;
        _physicsTimeSec = 0.0f;
        _networkTimeSec = 0.0f;
        _overlapTimeSec = 0.0f;
        _combatTimeSec = 0.0f;
        _updateCount = 0;
        _maxUpdateSec = 0.0f;
        _lastPerfLogTime = _totalRunTime;
    }
}

void Room::UpdateObject(
    const std::shared_ptr<GameObject> &obj, float deltaTime, std::vector<Protocol::ObjectPos> &monsterMoves,
    std::vector<Protocol::ObjectPos> &playerMoves
)
{
    // AI / Logic Update
    obj->Update(deltaTime, this);

    float oldX = obj->GetX();
    float oldY = obj->GetY();

    // Simple Euler Integration
    // Server uses input-based velocity (from C_MOVE_INPUT)
    if (std::abs(obj->GetVX()) > std::numeric_limits<float>::epsilon() ||
        std::abs(obj->GetVY()) > std::numeric_limits<float>::epsilon())
    {
        float newX = oldX + obj->GetVX() * deltaTime;
        float newY = oldY + obj->GetVY() * deltaTime;

        // Map Bounds Check (TODO)

        obj->SetPos(newX, newY);
        // _grid.Update(obj); // [Optimization] 이제 틱당 한 번 Rebuild하므로 여기서 호출 안 함
    }

    Protocol::ObjectPos move;
    move.set_object_id(obj->GetId());
    move.set_x(obj->GetX());
    move.set_y(obj->GetY());
    move.set_vx(obj->GetVX());
    move.set_vy(obj->GetVY());

    if (obj->GetType() == Protocol::ObjectType::PLAYER)
    {
        playerMoves.push_back(move);
    }
    else
    {
        monsterMoves.push_back(move);
    }
}

void Room::BroadcastPlayerMoves(const std::vector<Protocol::ObjectPos> &playerMoves)
{
    if (playerMoves.empty())
        return;

    for (auto &pair : _players)
    {
        auto viewer = pair.second;
        if (!viewer->IsReady())
            continue;

        Protocol::S_MoveObjectBatch batch;
        batch.set_server_tick(_serverTick);

        for (const auto &move : playerMoves)
        {
            if (move.object_id() != viewer->GetId())
                *batch.add_moves() = move;
        }

        if (batch.moves_size() > 0)
        {
            S_MoveObjectBatchPacket pkt(batch);
            SendToPlayer(pair.first, pkt);
        }
    }
}

void Room::SendPlayerAcks()
{
    for (auto &pair : _players)
    {
        auto player = pair.second;

        Protocol::S_PlayerStateAck ack;
        ack.set_server_tick(_serverTick);
        ack.set_client_tick(player->GetLastProcessedClientTick());
        ack.set_x(player->GetX());
        ack.set_y(player->GetY());

        S_PlayerStateAckPacket pkt(ack);
        SendToPlayer(pair.first, pkt);

        // Update Internal LastSent State to prevent resending if nothing changes
        player->UpdateLastSentState(_totalRunTime, _serverTick);
    }
}

std::shared_ptr<Player> Room::GetNearestPlayer(float x, float y)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    std::shared_ptr<Player> nearest = nullptr;
    float minDistSq = std::numeric_limits<float>::max();

    for (auto &pair : _players)
    {
        auto p = pair.second;
        if (p->IsDead())
            continue;

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

std::vector<std::shared_ptr<Monster>> Room::GetMonstersInRange(float x, float y, float radius)
{
    _monsterBuffer.clear();
    _queryBuffer.clear();

    // SpatialGrid is used for optimized query
    _grid.QueryRange(x, y, radius, _queryBuffer, _objMgr);
    for (auto &obj : _queryBuffer)
    {
        if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {
            auto monster = std::dynamic_pointer_cast<Monster>(obj);
            if (monster != nullptr && !monster->IsDead())
            {
                _monsterBuffer.push_back(monster);
            }
        }
    }
    return _monsterBuffer;
}

Vector2
Room::GetSeparationVector(float x, float y, float radius, int32_t excludeId, const Vector2 &velocity, int maxNeighbors)
{
    Vector2 separation(0.0f, 0.0f);
    int count = 0;

    // Directional check optimization
    bool useDirectionalCheck = !velocity.IsZero();

    _queryBuffer.clear();
    _grid.QueryRange(x, y, radius, _queryBuffer, _objMgr);
    for (auto &obj : _queryBuffer)
    {
        if (obj->GetId() == excludeId || obj->GetType() != Protocol::ObjectType::MONSTER)
            continue;

        float dx = x - obj->GetX();
        float dy = y - obj->GetY();

        // [Optimization] Directional Filter: Ignore monsters behind me
        // Only if I am moving
        if (useDirectionalCheck)
        {
            // Vector pointing to neighbor from me is (-dx, -dy)
            // But we want to check if the neighbor is "in front" of my velocity.
            // If neighbor is relative pos P_other, my pos P_me.
            // Relative vector T = P_other - P_me = (obj->GetX() - x, obj->GetY() - y) = (-dx, -dy)
            // Dot(velocity, T) > 0 means in front (roughly).
            // User code: if (toOther.Dot(velocity) < 0) continue; // Behind

            float toOtherX = obj->GetX() - x;
            float toOtherY = obj->GetY() - y;
            // No need to normalize for sign check
            if ((velocity.x * toOtherX + velocity.y * toOtherY) < 0)
                continue;
        }

        float distSq = dx * dx + dy * dy;
        float radiusSq = radius * radius;

        if (distSq > 0.0001f && distSq < radiusSq)
        {
            float dist = std::sqrt(distSq);
            float invDist = 1.0f / dist;

            // Closer = Stronger weight (1.0 - dist/radius)
            float weight = 1.0f - (dist / radius);

            // Normalized direction (dx * invDist) * weight
            separation.x += dx * (invDist * weight);
            separation.y += dy * (invDist * weight);
            count++;

            if (count >= maxNeighbors)
                break;
        }
    }

    if (count > 0)
    {
        // Don't normalize yet, let the force magnitude speak
        // but clamp to manageable levels
        float magSq = separation.MagnitudeSq();
        if (magSq > 1.0f)
        {
            float mag = std::sqrt(magSq);
            separation.x /= mag;
            separation.y /= mag;
        }
    }

    return separation;
}

// [Phase 3] Broadcast Optimization
void Room::BroadcastPacket(const System::IPacket &pkt, uint64_t excludeSessionId)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    for (auto &pair : _players)
    {
        if (pair.first == excludeSessionId)
            continue;

        auto &player = pair.second;
        if (!player->IsReady())
            continue;

        SendToPlayer(pair.first, pkt);
    }
}

void Room::SendToPlayer(uint64_t sessionId, const System::IPacket &pkt)
{
    if (_dispatcher)
    {
        // [Crash Fix] Use PacketPtr (RAII) to ensure packet lifetime in async calls
        // Pre-serialize here to stack/heap (PacketBuilder uses MessagePool)
        // PacketPtr manages ownership and ref-counting automatically.
        System::PacketPtr msg = System::PacketBuilder::BuildShared(pkt);

        _dispatcher->WithSession(
            sessionId,
            [msg](System::SessionContext &ctx)
            {
                // msg is copied by value (IncRef)
                // When lambda executes, msg stays alive.
                // ctx.Send takes msg (shares ownership or copies ref)
                ctx.Send(msg);
            }
        );
    }
}

size_t Room::GetPlayerCount()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _players.size();
}

void Room::HandleGameOver(bool isWin)
{
    if (_isGameOver)
        return;

    _isGameOver = true;
    LOG_INFO("Game Over in Room {} (Win: {})", _roomId, isWin);

    Protocol::S_GameOver msg;
    msg.set_survived_time_ms(static_cast<int64_t>(_totalRunTime * 1000));
    msg.set_is_win(isWin);

    BroadcastPacket(S_GameOverPacket(std::move(msg)));
}

bool Room::CheckWinCondition() const
{
    return _waveMgr.IsAllWavesComplete() && _objMgr.GetAliveMonsterCount() == 0;
}

void Room::DebugAddExpToAll(int32_t exp)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    for (auto &pair : _players)
    {
        auto player = pair.second;
        player->AddExp(exp, this);
    }
    LOG_INFO("DEBUG: Added {} EXP to all players in Room {}", exp, _roomId);
}

void Room::DebugSpawnMonster(int32_t monsterId, int32_t count)
{
    // Delegate to WaveManager
    _waveMgr.DebugSpawn(this, monsterId, count);
}

void Room::DebugToggleGodMode()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    for (auto &pair : _players)
    {
        auto player = pair.second;
        bool newState = !player->IsGodMode();
        player->SetGodMode(newState);
    }
    LOG_INFO("DEBUG: Toggled God Mode for all players in Room {}", _roomId);
}

} // namespace SimpleGame
