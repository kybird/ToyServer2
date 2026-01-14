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
    WaveManager(ObjectManager &objMgr, SpatialGrid &grid, int roomId) : _objMgr(objMgr), _grid(grid), _roomId(roomId)
    {
    }

    void Start();
    void Update(float dt, Room *room);
    void Reset(); // Reset wave manager state

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

    void StartSpawner(const WaveData &wave);
    void SpawnMonster(int32_t monsterTypeId, float hpMultiplier, Room *room);
    void BroadcastProto(Room *room, PacketID id, const Protocol::S_SpawnObject &msg);

    ObjectManager &_objMgr;
    SpatialGrid &_grid;
    int _roomId;

    float _currentTime = 0.0f;
    std::vector<WaveData> _waves;
    int _currentWaveIndex = 0;
    float _nextSpawnTime = 0.0f;

    std::vector<PeriodicSpawner> _activeSpawners;
};

} // namespace SimpleGame
