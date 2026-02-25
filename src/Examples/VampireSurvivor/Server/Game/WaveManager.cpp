#include "WaveManager.h"
#include "Entity/MonsterFactory.h"
#include "Game/Room.h"
#include "GameConfig.h"
#include "GamePackets.h"
#include "System/ILog.h"
#include "System/Utility/FastRandom.h"

namespace SimpleGame {

void WaveManager::Start()
{
    _state = WaveState::InProgress;
    _currentTime = 0.0f;
    _currentWaveIndex = 0;
    _nextSpawnTime = 0.0f;
    _waves = DataManager::Instance().GetWaves();
    LOG_INFO("WaveManager Started for Room {}", _roomId);
}

void WaveManager::Reset()
{
    _state = WaveState::NotStarted;
    _currentTime = 0.0f;
    _currentWaveIndex = 0;
    _nextSpawnTime = 0.0f;
    _activeSpawners.clear();

    // FIX: 웨이브 데이터 재로드
    _waves = DataManager::Instance().GetWaves();

    LOG_INFO("WaveManager reset for Room {} (reloaded {} waves)", _roomId, _waves.size());
}

void WaveManager::Update(float dt, Room *room)
{
    _currentTime += dt;

    // Check for new waves or spawning
    while (_currentWaveIndex < _waves.size())
    {
        const auto &wave = _waves[_currentWaveIndex];

        if (_currentTime >= wave.startTime)
        {
            StartSpawner(room, wave);
            _currentWaveIndex++;
        }
        else
        {
            break; // Future waves
        }
    }

    // Build Clusters for this frame (Optimization: Could be done once per frame if expensive)
    auto clusters = BuildClusters(room);
    int totalPlayers = 0;
    for (const auto &c : clusters)
        totalPlayers += c.players.size();

    // Update Active Spawners
    for (auto it = _activeSpawners.begin(); it != _activeSpawners.end();)
    {
        it->timer -= dt;
        it->remainingDuration -= dt;

        if (it->timer <= 0)
        {
            // [Optimization] 현재 몬스터 수 체크
            size_t currentMonsterCount = room->GetObjectManager().GetAliveMonsterCount();

            if (currentMonsterCount >= GameConfig::MAX_MONSTERS_PER_ROOM)
            {
                // 제한 초과 시 스폰 안 함
                it->timer = it->interval; // 다음 틱에 다시 시도
                ++it;
                continue;
            }

            if (totalPlayers > 0)
            {
                // 스폰 가능한 수량 계산
                int maxSpawnable = static_cast<int>(GameConfig::MAX_MONSTERS_PER_ROOM - currentMonsterCount);
                int actualBatch = std::min(it->batchCount, maxSpawnable);

                // Distribute batch count among clusters
                int remainingBatch = actualBatch;

                for (const auto &cluster : clusters)
                {
                    if (remainingBatch <= 0)
                        break;

                    // Proportional spawn count
                    int clusterSpawnCount = (int)((float)cluster.players.size() / totalPlayers * actualBatch);
                    if (clusterSpawnCount == 0 && remainingBatch > 0)
                        clusterSpawnCount = 1;
                    if (clusterSpawnCount > remainingBatch)
                        clusterSpawnCount = remainingBatch;

                    for (int i = 0; i < clusterSpawnCount; ++i)
                    {
                        auto pos = GetAngularGapSpawnPos(cluster);
                        SpawnMonster(it->monsterTypeId, it->hpMultiplier, room, pos.first, pos.second);
                    }

                    remainingBatch -= clusterSpawnCount;
                }
            }

            it->timer = it->interval;
        }

        if (it->remainingDuration <= 0)
        {
            it = _activeSpawners.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // [NEW] 1회성 상태 전이: InProgress → Completed
    if (_state == WaveState::InProgress && _currentWaveIndex >= _waves.size() && _activeSpawners.empty())
    {
        _state = WaveState::Completed;
        LOG_INFO("All waves completed in Room {}", _roomId);
    }
}

std::vector<WaveManager::PlayerCluster> WaveManager::BuildClusters(Room *room) const
{
    std::vector<PlayerCluster> clusters;
    const float CLUSTER_RADIUS =
        GameConfig::MONSTER_SPAWN_CLUSTER_RADIUS; // Adjust based on screen size (e.g. Screen + 20%)

    // Get All Players
    // Note: We need a way to get players from Room.
    // Since we don't have direct access to Room::_players map, we use ObjectManager or need Room to expose it.
    // Assuming ObjectManager contains players.
    std::vector<std::shared_ptr<GameObject>> allPlayers;
    auto objects = _objMgr.GetAllObjects();
    for (auto &obj : objects)
    {
        if (obj->GetType() == Protocol::ObjectType::PLAYER)
        {
            auto player = std::dynamic_pointer_cast<Player>(obj);
            if (player && !player->IsDead())
            {
                allPlayers.push_back(obj);
            }
        }
    }

    if (allPlayers.empty())
        return clusters;

    // Simple Clustering O(N^2) - N is small (<=4)
    for (const auto &p : allPlayers)
    {
        bool added = false;
        for (auto &c : clusters)
        {
            for (auto &existing : c.players)
            {
                float dx = p->GetX() - existing->GetX();
                float dy = p->GetY() - existing->GetY();
                if (dx * dx + dy * dy <= CLUSTER_RADIUS * CLUSTER_RADIUS)
                {
                    c.players.push_back(p);
                    // Update Center (Basic average update)
                    // Re-averaging is simpler after loop, but iterative is fine for small groups
                    added = true;
                    break;
                }
            }
            if (added)
                break;
        }

        if (!added)
        {
            PlayerCluster newC;
            newC.players.push_back(p);
            clusters.push_back(newC);
        }
    }

    // Finalize Centers
    for (auto &c : clusters)
    {
        float sx = 0, sy = 0;
        for (auto &p : c.players)
        {
            sx += p->GetX();
            sy += p->GetY();
        }
        c.centerX = sx / c.players.size();
        c.centerY = sy / c.players.size();
    }

    return clusters;
}

std::pair<float, float> WaveManager::GetAngularGapSpawnPos(const PlayerCluster &cluster) const
{
    static const float MIN_SPAWN_DIST = GameConfig::MONSTER_SPAWN_MIN_DIST; // Base distance from center
    static const float MAX_SPAWN_DIST = GameConfig::MONSTER_SPAWN_MAX_DIST; // Cap

    // 1. Calculate Max Player Dist from Center to define Spawn Radius
    float maxPlayerDist = 0.0f;
    for (auto &p : cluster.players)
    {
        float dx = p->GetX() - cluster.centerX;
        float dy = p->GetY() - cluster.centerY;
        maxPlayerDist = std::max(maxPlayerDist, std::sqrt(dx * dx + dy * dy));
    }

    float spawnRadius = std::min(maxPlayerDist + MIN_SPAWN_DIST, MAX_SPAWN_DIST);

    // 2. Angular Gap Logic
    float spawnAngle = 0.0f;
    static thread_local System::Utility::FastRandom rng;

    if (cluster.players.size() <= 1)
    {
        // Random angle if single player
        spawnAngle = rng.NextFloat(0.0f, 2.0f * 3.14159f);
    }
    else
    {
        // Collect angles
        std::vector<float> angles;
        for (auto &p : cluster.players)
        {
            angles.push_back(std::atan2(p->GetY() - cluster.centerY, p->GetX() - cluster.centerX));
        }
        std::sort(angles.begin(), angles.end());

        // Find max gap
        float maxGap = 0.0f;
        float bestAngle = angles[0];

        // Check gaps between adjacent
        for (size_t i = 0; i < angles.size(); ++i)
        {
            float a1 = angles[i];
            float a2 = angles[(i + 1) % angles.size()];
            float diff = a2 - a1;
            if (diff < 0)
                diff += 2 * 3.14159f; // Wrap around case

            if (diff > maxGap)
            {
                maxGap = diff;
                // Mid angle
                float mid = a1 + diff * 0.5f;
                bestAngle = mid;
            }
        }
        spawnAngle = bestAngle;
    }

    // 3. Calc Position with Jitter
    // Add larger random jitter to radius and angle to prevent stacking
    // Wider angle spread (+/- 0.8 rad ~= +/- 45 deg) and deeper radius variation (0~5m)
    float jitterAngle = rng.NextFloat(-0.8f, 0.8f);
    float finalAngle = spawnAngle + jitterAngle;

    float jitterDist = rng.NextFloat(0.0f, 5.0f);
    float finalRadius = spawnRadius + jitterDist;

    return {cluster.centerX + std::cos(finalAngle) * finalRadius, cluster.centerY + std::sin(finalAngle) * finalRadius};
}

void WaveManager::StartSpawner(Room *room, const WaveInfo &wave)
{
    PeriodicSpawner spawner;
    spawner.monsterTypeId = wave.monsterTypeId;
    spawner.batchCount = wave.count;
    spawner.hpMultiplier = wave.hpMultiplier;
    spawner.remainingDuration = wave.duration;
    spawner.interval = wave.interval;
    spawner.timer = 0.0f; // Spawn immediately
    _activeSpawners.push_back(spawner);

    // Notify Clients
    Protocol::S_WaveNotify notify;
    notify.set_wave_index(_currentWaveIndex + 1);
    notify.set_title("Wave " + std::to_string(_currentWaveIndex + 1));
    notify.set_duration_seconds(wave.duration);

    S_WaveNotifyPacket pkt(notify);
    room->BroadcastPacket(pkt);
    LOG_INFO("Started Wave {} in Room {}", _currentWaveIndex + 1, _roomId);
}

void WaveManager::SpawnMonster(int32_t monsterTypeId, float hpMultiplier, Room *room, float x, float y)
{
    // [Optimization] 최대 몬스터 수 재확인 (상향: 500)
    size_t currentMonsterCount = room->GetObjectManager().GetAliveMonsterCount();
    if (currentMonsterCount >= GameConfig::MAX_MONSTERS_PER_ROOM)
    {
        return;
    }

    // Apply HP Multiplier from WaveData
    int32_t finalHp = 0;
    int32_t maxHp = 100;
    const auto *tmpl = DataManager::Instance().GetMonsterInfo(monsterTypeId);
    if (tmpl)
    {
        finalHp = static_cast<int32_t>(tmpl->hp * hpMultiplier);
        maxHp = tmpl->hp;
    }

    // GameObject 시스템 사용 (단일 경로)
    auto monster = MonsterFactory::Instance().CreateMonster(_objMgr, monsterTypeId, x, y, finalHp);
    if (monster)
    {
        _objMgr.AddObject(monster);

        // Broadcast Spawn
        Protocol::S_SpawnObject msg;
        msg.set_server_tick(room->GetServerTick());
        auto *info = msg.add_objects();
        info->set_object_id(monster->GetId());
        info->set_type(Protocol::ObjectType::MONSTER);
        info->set_type_id(monster->GetMonsterTypeId());
        info->set_x(x);
        info->set_y(y);
        info->set_hp(monster->GetHp());
        info->set_max_hp(monster->GetMaxHp());
        info->set_state(Protocol::ObjectState::IDLE);

        BroadcastProto(room, PacketID::S_SPAWN_OBJECT, msg);
    }
}

// Helper removed in favor of direct packet creation in SpawnMonster or Update BroadcastProto
void WaveManager::BroadcastProto(Room *room, PacketID id, const Protocol::S_SpawnObject &msg)
{
    if (id == PacketID::S_SPAWN_OBJECT)
    {
        S_SpawnObjectPacket packet(msg);
        room->BroadcastPacket(packet);
    }
}

void WaveManager::DebugSpawn(Room *room, int32_t monsterTypeId, int32_t count)
{
    // Need a position near players
    auto clusters = BuildClusters(room);
    if (clusters.empty())
    {
        // No players, spawn at 0,0?
        for (int i = 0; i < count; ++i)
        {
            SpawnMonster(monsterTypeId, 1.0f, room, 0.0f, 0.0f);
        }
        return;
    }

    // Distribute among clusters or just pick first
    static thread_local System::Utility::FastRandom rng;
    for (int i = 0; i < count; ++i)
    {
        size_t index = static_cast<size_t>(rng.NextInt(0, static_cast<int>(clusters.size()) - 1));
        const auto &cluster = clusters[index];
        auto pos = GetAngularGapSpawnPos(cluster);
        SpawnMonster(monsterTypeId, 1.0f, room, pos.first, pos.second);
    }

    LOG_INFO("DebugSpawn: Spawned {} monsters of type {} in Room {}", count, monsterTypeId, _roomId);
}

} // namespace SimpleGame
