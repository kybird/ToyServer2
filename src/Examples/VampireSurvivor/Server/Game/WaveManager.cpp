#include "WaveManager.h"
#include "Entity/MonsterFactory.h"
#include "Game/Room.h"
#include "GameConfig.h"
#include "GamePackets.h"
#include "System/ILog.h"


namespace SimpleGame {

void WaveManager::Start()
{
    _currentTime = 0.0f;
    _currentWaveIndex = 0;
    _nextSpawnTime = 0.0f;
    _waves = DataManager::Instance().GetWaves();
    LOG_INFO("WaveManager Started for Room {}", _roomId);
}

void WaveManager::Reset()
{
    _currentTime = 0.0f;
    _currentWaveIndex = 0;
    _nextSpawnTime = 0.0f;
    _activeSpawners.clear();
    LOG_INFO("WaveManager reset for Room {}", _roomId);
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
            StartSpawner(wave);
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
            // [CPU Optimization] Cap monster population
            static const size_t MAX_MONSTERS_PER_ROOM = 500;
            size_t currentCount = _objMgr.GetObjectCount(); // Approximate

            if (currentCount < MAX_MONSTERS_PER_ROOM && totalPlayers > 0)
            {
                // Distribute batch count among clusters
                int remainingBatch = it->batchCount;

                for (const auto &cluster : clusters)
                {
                    if (remainingBatch <= 0)
                        break;

                    // Proportional spawn count
                    // Minimal 1 per cluster if batch allows, else proportional
                    int clusterSpawnCount = (int)((float)cluster.players.size() / totalPlayers * it->batchCount);
                    if (clusterSpawnCount == 0 && remainingBatch > 0)
                        clusterSpawnCount = 1; // Ensure at least one if possible
                    if (clusterSpawnCount > remainingBatch)
                        clusterSpawnCount = remainingBatch;

                    // Spawn for this cluster
                    // We can reuse the spawn pos for a few monsters or recalculate
                    // For "Gap" logic, usually we want to fill gaps.
                    // Let's recalculate or scatter slightly for each monster?
                    // To be safe and fill gaps, let's just pick a random gap-based pos for EACH monster.
                    // Or efficient: Pick central gap pos and jitter.
                    // User said: "Angular Gap Spawn is a cluster logic".

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

    if (cluster.players.size() <= 1)
    {
        // Random angle if single player
        spawnAngle = ((rand() % 360) / 180.0f) * 3.14159f;
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
    // Add small random jitter to radius and angle to prevent stacking
    float jitterAngle = ((rand() % 100) / 100.0f - 0.5f) * 0.5f; // +/- 0.25 rad
    float finalAngle = spawnAngle + jitterAngle;

    float jitterDist = ((rand() % 100) / 100.0f) * 2.0f; // 0~2m variation
    float finalRadius = spawnRadius + jitterDist;

    return {cluster.centerX + std::cos(finalAngle) * finalRadius, cluster.centerY + std::sin(finalAngle) * finalRadius};
}

void WaveManager::StartSpawner(const WaveData &wave)
{
    PeriodicSpawner spawner;
    spawner.monsterTypeId = wave.monsterTypeId;
    spawner.batchCount = wave.count;
    spawner.hpMultiplier = wave.hpMultiplier;
    spawner.remainingDuration = wave.duration;
    spawner.interval = wave.interval;
    spawner.timer = 0.0f; // Spawn immediately
    _activeSpawners.push_back(spawner);
}

void WaveManager::SpawnMonster(int32_t monsterTypeId, float hpMultiplier, Room *room, float x, float y)
{
    // Apply HP Multiplier from WaveData
    int32_t finalHp = 0;
    const auto *tmpl = DataManager::Instance().GetMonsterTemplate(monsterTypeId);
    if (tmpl)
    {
        finalHp = static_cast<int32_t>(tmpl->hp * hpMultiplier);
    }

    auto monster = MonsterFactory::Instance().CreateMonster(_objMgr, monsterTypeId, x, y, finalHp);
    if (monster)
    {
        _objMgr.AddObject(monster);
        _grid.Add(monster);

        // Broadcast Spawn
        Protocol::S_SpawnObject msg;
        msg.set_server_tick(room->GetServerTick()); // [중요] 틱 동기화를 위해 server_tick 설정
        auto *info = msg.add_objects();
        info->set_object_id(monster->GetId());
        info->set_type(Protocol::ObjectType::MONSTER);
        info->set_type_id(monster->GetMonsterTypeId());
        info->set_x(x);
        info->set_y(y);
        info->set_hp(monster->GetHp());
        info->set_max_hp(monster->GetMaxHp());
        info->set_state(Protocol::ObjectState::IDLE); // Explicit state

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

} // namespace SimpleGame
