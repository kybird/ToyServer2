#pragma once
#include "Core/DataManager.h"
#include "Entity/MonsterFactory.h"
#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"
#include "Core/Protocol.h"

namespace SimpleGame {

class Room; // Forward Declaration

class WaveManager {
public:
    WaveManager(ObjectManager& objMgr, SpatialGrid& grid, int roomId) 
        : _objMgr(objMgr), _grid(grid), _roomId(roomId) 
    {
    }

    void Start();
    void Update(float dt, Room* room);

private:
    struct PeriodicSpawner {
        int32_t monsterTypeId;
        int32_t totalCount;
        int32_t spawnedCount;
        float interval;
        float timer;
    };

    void StartSpawner(const WaveData& wave);
    void SpawnMonster(const PeriodicSpawner& spawner, Room* room);
    void BroadcastProto(Room* room, PacketID id, const Protocol::S_SpawnObject& msg);

    ObjectManager& _objMgr;
    SpatialGrid& _grid;
    int _roomId;
    
    float _currentTime = 0.0f;
    std::vector<WaveData> _waves;
    int _currentWaveIndex = 0;
    float _nextSpawnTime = 0.0f;
    
    std::vector<PeriodicSpawner> _activeSpawners;
};

} // namespace SimpleGame

