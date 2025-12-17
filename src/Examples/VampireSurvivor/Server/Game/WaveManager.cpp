#include "WaveManager.h"
#include "Room.h"
#include "../Entity/MonsterFactory.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Network/PacketUtils.h"

namespace SimpleGame {

void WaveManager::Start() {
    _currentTime = 0.0f;
    _currentWaveIndex = 0;
    _nextSpawnTime = 0.0f;
    _waves = DataManager::Instance().GetWaves();
    LOG_INFO("WaveManager Started for Room {}", _roomId);
}

void WaveManager::Update(float dt, Room* room) {
    _currentTime += dt;

    // Check for new waves or spawning
    while (_currentWaveIndex < _waves.size()) {
        const auto& wave = _waves[_currentWaveIndex];
        
        if (_currentTime >= wave.startTime) {
            StartSpawner(wave);
            _currentWaveIndex++;
        } else {
            break; // Future waves
        }
    }

    // Update Active Spawners
    for (auto it = _activeSpawners.begin(); it != _activeSpawners.end(); ) {
         it->timer -= dt;
         if (it->timer <= 0) {
             SpawnMonster(*it, room);
             it->spawnedCount++;
             it->timer = it->interval;
         }

         if (it->spawnedCount >= it->totalCount) {
             it = _activeSpawners.erase(it);
         } else {
             ++it;
         }
    }
}

void WaveManager::StartSpawner(const WaveData& wave) {
    PeriodicSpawner spawner;
    spawner.monsterTypeId = wave.monsterTypeId;
    spawner.totalCount = wave.count;
    spawner.spawnedCount = 0;
    spawner.interval = wave.interval;
    spawner.timer = 0.0f; // Spawn immediately
    _activeSpawners.push_back(spawner);
}

void WaveManager::SpawnMonster(const PeriodicSpawner& spawner, Room* room) {
    // Random Position around 0,0 (or players)
    // For MVP: Random around 0,0 within 20m
    float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
    float dist = 5.0f + (rand() % 1500) / 100.0f; // 5 to 20
    float x = cos(angle) * dist;
    float y = sin(angle) * dist;

    auto monster = MonsterFactory::Instance().CreateMonster(_objMgr, spawner.monsterTypeId, x, y);
    if (monster) {
        _objMgr.AddObject(monster);
        _grid.Add(monster);
        
        // Broadcast Spawn
        Protocol::S_SpawnObject msg;
        auto* info = msg.add_objects();
        info->set_object_id(monster->GetId());
        info->set_type(Protocol::ObjectType::MONSTER);
        info->set_type_id(monster->GetMonsterTypeId());
        info->set_x(x);
        info->set_y(y);
        info->set_hp(monster->GetHp());
        info->set_max_hp(monster->GetMaxHp());
        
        BroadcastProto(room, PacketID::S_SPAWN_OBJECT, msg);
    }
}

void WaveManager::BroadcastProto(Room* room, PacketID id, const Protocol::S_SpawnObject& msg) {
     size_t bodySize = msg.ByteSizeLong();
     auto* packet = System::MessagePool::AllocatePacket((uint16_t)bodySize);
     PacketHeader* header = (PacketHeader*)packet->Payload();
     header->size = (uint16_t)(sizeof(PacketHeader) + bodySize);
     header->id = (uint16_t)id;
     msg.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)bodySize);
     room->Broadcast(packet);
     System::PacketUtils::ReleasePacket(packet);
}

} // namespace SimpleGame
