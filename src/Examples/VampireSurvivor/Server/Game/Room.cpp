#include "Game/Room.h"
#include "Core/UserDB.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Game/DamageEmitter.h"
#include "Game/GameConfig.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/Packet/IPacket.h"
#include "System/Packet/PacketBroadcast.h"
#include "System/Thread/IStrand.h"
#include <algorithm>
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
        // Use constants for TPS and interval
        _timerHandle = _timer->SetInterval(1, GameConfig::TICK_INTERVAL_MS, this);
        std::cout << "[DEBUG] Room " << _roomId << " Game Loop Started (" << GameConfig::TPS << " TPS, "
                  << GameConfig::TICK_INTERVAL_MS << "ms). TimerHandle: " << _timerHandle << std::endl;
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
    _serverTick++;

    // LOG_DEBUG("[ServerTick] {}", _serverTick);
    if (_strand)
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

// Helper to broadcast Protobuf message - Overloaded for specific packets
void BroadcastProto(Room *room, const Protocol::S_SpawnObject &msg)
{
    S_SpawnObjectPacket packet(msg);
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, Protocol::S_SpawnObject &&msg)
{
    S_SpawnObjectPacket packet(std::move(msg));
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, const Protocol::S_DespawnObject &msg)
{
    S_DespawnObjectPacket packet(msg);
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, Protocol::S_DespawnObject &&msg)
{
    S_DespawnObjectPacket packet(std::move(msg));
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, const Protocol::S_MoveObjectBatch &msg)
{
    S_MoveObjectBatchPacket packet(msg);
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, Protocol::S_MoveObjectBatch &&msg)
{
    S_MoveObjectBatchPacket packet(std::move(msg));
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, const Protocol::S_GameOver &msg)
{
    S_GameOverPacket packet(msg);
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, Protocol::S_GameOver &&msg)
{
    S_GameOverPacket packet(std::move(msg));
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, const Protocol::S_PlayerDead &msg)
{
    S_PlayerDeadPacket packet(msg);
    room->BroadcastPacket(packet);
}

void BroadcastProto(Room *room, Protocol::S_PlayerDead &&msg)
{
    S_PlayerDeadPacket packet(std::move(msg));
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
    BroadcastProto(this, std::move(msg));
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
    BroadcastProto(this, std::move(msg));
}

void Room::Enter(std::shared_ptr<Player> player)
{
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
    int userId = (int)player->GetSessionId(); // TODO: Use real UserId
    auto self = shared_from_this();           // Keep Room alive during DB call

    if (_userDB)
    {
        _userDB->GetUserSkills(
            userId,
            [self, player](std::vector<std::pair<int, int>> skills)
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

                // Create Default DamageEmitter for Player
                // Use skillId 1 for base_linear
                self->_emitters.push_back(std::make_unique<DamageEmitter>(1, player));
            }
        );
    }
    else
    {
        // No DB? Just enter immediately
        std::lock_guard<std::recursive_mutex> lock(_mutex);
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

    BroadcastProto(this, std::move(newPlayerSpawn));
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
        BroadcastProto(this, std::move(despawn));

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

    _waveMgr.Update(deltaTime, this);
    _totalRunTime += deltaTime;

    // 1. Physics & Logic Update
    auto objects = _objMgr.GetAllObjects();

    // Separate updates for batching
    std::vector<Protocol::ObjectPos> monsterMoves;
    std::vector<Protocol::ObjectPos> playerMoves;

    for (auto &obj : objects)
    {
        // AI / Logic Update
        obj->Update(deltaTime, this);

        float oldX = obj->GetX();
        float oldY = obj->GetY();

        // Simple Euler Integration
        // Server uses input-based velocity (from C_MOVE_INPUT)
        if (obj->GetVX() != 0 || obj->GetVY() != 0)
        {
            float newX = oldX + obj->GetVX() * deltaTime;
            float newY = oldY + obj->GetVY() * deltaTime;

            // Map Bounds Check (TODO)

            obj->SetPos(newX, newY);

            if (obj->GetType() == Protocol::ObjectType::PLAYER)
            {
                // LOG_DEBUG(
                //     "[UpdateTick] Tick={} Obj={} Pos=({:.2f},{:.2f}) -> ({:.2f},{:.2f}) Vel=({:.2f},{:.2f})",
                //     _serverTick,
                //     obj->GetId(),
                //     oldX,
                //     oldY,
                //     newX,
                //     newY,
                //     obj->GetVX(),
                //     obj->GetVY()
                // );
            }

            _grid.Update(obj, oldX, oldY);
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

    // === 2. Broadcast Monsters ===
    if (!monsterMoves.empty())
    {
        Protocol::S_MoveObjectBatch batch;
        batch.set_server_tick(_serverTick);

        for (auto &move : monsterMoves)
            *batch.add_moves() = move;

        BroadcastProto(this, std::move(batch));
    }

    // === 3. Broadcast Players (filter self) ===
    if (!playerMoves.empty())
    {
        for (auto &pair : _players)
        {
            auto viewer = pair.second;
            if (!viewer->GetSession())
                continue;

            Protocol::S_MoveObjectBatch batch;
            batch.set_server_tick(_serverTick);

            for (auto &move : playerMoves)
            {
                if (move.object_id() != viewer->GetId())
                    *batch.add_moves() = move;
            }

            if (batch.moves_size() > 0)
            {
                S_MoveObjectBatchPacket pkt(batch);
                viewer->GetSession()->SendPacket(pkt);
            }
        }
    }

    // === 4. Send Player Ack (authoritative snapshot) ===
    for (auto &pair : _players)
    {
        auto player = pair.second;
        if (!player->GetSession())
            continue;

        Protocol::S_PlayerStateAck ack;
        ack.set_server_tick(_serverTick);
        ack.set_client_tick(player->GetLastProcessedClientTick());
        ack.set_x(player->GetX());
        ack.set_y(player->GetY());

        S_PlayerStateAckPacket pkt(ack);
        player->GetSession()->SendPacket(pkt);

        // Update Internal LastSent State to prevent resending if nothing changes
        player->UpdateLastSentState(_totalRunTime, _serverTick);

        // LOG_DEBUG(
        //     "[Ack] Player={} ServerTick={} ClientTick={} Pos=({:.2f},{:.2f})",
        //     player->GetId(),
        //     _serverTick,
        //     player->GetLastProcessedClientTick(),
        //     player->GetX(),
        //     player->GetY()
        // );
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
                            // Give exp to projectile owner
                            auto it = _players.find(proj->GetOwnerId());
                            if (it != _players.end())
                            {
                                it->second->AddExp(10, this);
                            }
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
        S_DamageEffectPacket damagePkt(std::move(damageEffect));
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

    // 5. Update Damage Emitters (Player Attacks)
    for (auto &emitter : _emitters)
    {
        emitter->Update(deltaTime, this);
    }

    _emitters.erase(
        std::remove_if(
            _emitters.begin(),
            _emitters.end(),
            [](const auto &e)
            {
                return e->IsExpired();
            }
        ),
        _emitters.end()
    );

    auto allObjectsForDamage = _objMgr.GetAllObjects();
    for (auto &obj : allObjectsForDamage)
    {
        if (obj->GetType() != Protocol::ObjectType::MONSTER)
            continue;

        auto monster = std::dynamic_pointer_cast<Monster>(obj);
        if (!monster || monster->IsDead())
            continue;

        for (auto &playerPair : _players)
        {
            auto player = playerPair.second;
            if (player->IsDead())
                continue;

            // Simple Circular Collision
            float dx = monster->GetX() - player->GetX();
            float dy = monster->GetY() - player->GetY();
            float distSq = dx * dx + dy * dy;
            float sumRad = monster->GetRadius() + player->GetRadius();

            if (distSq <= sumRad * sumRad)
            {
                // Skip collision if player is already dead
                if (player->IsDead())
                    continue;

                if (monster->CanAttack(_totalRunTime))
                {
                    player->TakeDamage(monster->GetContactDamage(), this);
                    monster->ResetAttackCooldown(_totalRunTime);

                    // Send HP Change Packet
                    {
                        Protocol::S_HpChange hpMsg;
                        hpMsg.set_object_id(player->GetId());
                        hpMsg.set_current_hp(player->GetHp());
                        hpMsg.set_max_hp(player->GetMaxHp());

                        S_HpChangePacket hpPkt(std::move(hpMsg));
                        // Send to everyone (so others can see HP bar updates if implemented) or just owner
                        // For now, let's broadcast to sync state (or change to unicast if optimized)
                        BroadcastPacket(hpPkt);
                    }

                    // Broadcast damage to player? (Optional: Damage Effect packet)
                    LOG_DEBUG("Monster {} hit Player {}. HP={}", monster->GetId(), player->GetId(), player->GetHp());

                    // Check if player died
                    if (player->IsDead())
                    {
                        LOG_INFO("Player {} has died in Room {}", player->GetId(), _roomId);

                        // 1. Notify everyone about player death
                        Protocol::S_PlayerDead deadMsg;
                        deadMsg.set_player_id(player->GetId());
                        BroadcastProto(this, std::move(deadMsg));

                        // 2. Check Game Over (Loss Condition)
                        bool allDead = true;
                        for (auto &pPair : _players)
                        {
                            if (!pPair.second->IsDead())
                            {
                                allDead = false;
                                break;
                            }
                        }

                        if (allDead)
                        {
                            LOG_INFO("All players died. Game Over in Room {}", _roomId);
                            Protocol::S_GameOver gameOverMsg;
                            gameOverMsg.set_survived_time_ms(static_cast<int64_t>(_totalRunTime * 1000));
                            gameOverMsg.set_is_win(false);
                            BroadcastProto(this, std::move(gameOverMsg));

                            // Optional: Reset room or transition to lobby
                            // For now, let's keep it in game-over state
                        }
                    }
                }
            }
        }
    }

    if (!despawnIds.empty())
    {
        BroadcastDespawn(despawnIds);
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
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    std::vector<std::shared_ptr<Monster>> found;

    if (true) // SpatialGrid is direct member, always there.
    {
        auto objects = _grid.QueryRange(x, y, radius);
        for (auto &obj : objects)
        {
            if (obj->GetType() == Protocol::ObjectType::MONSTER)
            {
                auto monster = std::dynamic_pointer_cast<Monster>(obj);
                if (monster && !monster->IsDead())
                {
                    found.push_back(monster);
                }
            }
        }
    }
    else
    {
        auto allObjectsInRange = _objMgr.GetAllObjects();
        for (auto &obj : allObjectsInRange)
        {
            if (obj->GetType() != Protocol::ObjectType::MONSTER)
                continue;

            auto monster = std::dynamic_pointer_cast<Monster>(obj);
            if (monster && !monster->IsDead())
            {
                float dx = monster->GetX() - x;
                float dy = monster->GetY() - y;
                float distSq = dx * dx + dy * dy;
                if (distSq <= radius * radius)
                {
                    found.push_back(monster);
                }
            }
        }
    }
    return found;
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
        {
            LOG_WARN("BroadcastPacket: Player {} has NO SESSION. Skipping.", pair.first);
            continue;
        }

        // Dynamic cast to check if it's a real Session (vs MockSession)
        // This overhead is acceptable compared to serialization savings for many players.
        auto *sess = dynamic_cast<System::Session *>(isess);
        if (sess)
        {
            // LOG_DEBUG("BroadcastPacket: Player {} is Real Session.", pair.first);
            realSessions.push_back(sess);
        }
        else
        {
            LOG_WARN("BroadcastPacket: Player {} is MockSession/Other. Fallback SendPacket.", pair.first);
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

void Player::AddExp(int32_t amount, Room *room)
{
    _exp += amount;

    // Level up check
    while (_exp >= _maxExp)
    {
        _exp -= _maxExp;
        _level++;
        _maxExp = 100 + (_level - 1) * 50; // Increase required exp per level
        LOG_INFO("Player {} leveled up to {}!", GetId(), _level);
    }

    // Send exp change to client
    if (_session && room)
    {
        Protocol::S_ExpChange expMsg;
        expMsg.set_current_exp(_exp);
        expMsg.set_max_exp(_maxExp);
        expMsg.set_level(_level);

        S_ExpChangePacket pkt(std::move(expMsg));
        _session->SendPacket(pkt);
    }
}

} // namespace SimpleGame
