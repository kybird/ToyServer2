#pragma once
#include "../Common/Protocol.h"
#include "Core/DataManager.h"
#include "Entity/MonsterFactory.h"
#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"

namespace SimpleGame {

class Room; // Forward Declaration

class WaveManager
{
public:
    enum class WaveState { NotStarted, InProgress, Completed };

    WaveManager(ObjectManager &objMgr, SpatialGrid &grid, int roomId) : _objMgr(objMgr), _grid(grid), _roomId(roomId)
    {
    }

    void Start();
    void Update(float dt, Room *room);
    void Reset(); // Reset wave manager state

    bool IsAllWavesComplete() const
    {
        return _state == WaveState::Completed;
    }

    // Debug
    void DebugSpawn(Room *room, int32_t monsterTypeId, int32_t count);

private:
    struct PeriodicSpawner
    {
        int32_t monsterTypeId;
        int32_t batchCount;
        float hpMultiplier;
        float interval;
        float timer;
        float remainingDuration;
    };

    void StartSpawner(Room *room, const WaveInfo &wave);
    void SpawnMonster(int32_t monsterTypeId, float hpMultiplier, Room *room);
    void BroadcastProto(Room *room, PacketID id, const Protocol::S_SpawnObject &msg);

    ObjectManager &_objMgr;
    SpatialGrid &_grid;
    int _roomId;

    float _currentTime = 0.0f;
    std::vector<WaveInfo> _waves;
    int _currentWaveIndex = 0;
    float _nextSpawnTime = 0.0f;

    std::vector<PeriodicSpawner> _activeSpawners;
    WaveState _state = WaveState::NotStarted;

    // Clustering & Spawning Logic
    struct PlayerCluster
    {
        std::vector<std::shared_ptr<GameObject>> players;
        float centerX = 0.0f;
        float centerY = 0.0f;
    };

    std::vector<PlayerCluster> BuildClusters(Room *room) const;
    std::pair<float, float> GetAngularGapSpawnPos(const PlayerCluster &cluster) const;
    void SpawnMonster(int32_t monsterTypeId, float hpMultiplier, Room *room, float x, float y);

}; // Missing semicolon fixed

} // namespace SimpleGame
