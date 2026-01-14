#include "WaveManager.h"
#include "Entity/MonsterFactory.h"
#include "Game/Room.h"
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

    // Update Active Spawners
    for (auto it = _activeSpawners.begin(); it != _activeSpawners.end();)
    {
        it->timer -= dt;
        it->remainingDuration -= dt;

        if (it->timer <= 0)
        {
            // [CPU Optimization] Cap monster population
            static const size_t MAX_MONSTERS_PER_ROOM = 500;

            for (int32_t i = 0; i < it->batchCount; ++i)
            {
                if (_objMgr.GetObjectCount() < MAX_MONSTERS_PER_ROOM)
                {
                    SpawnMonster(it->monsterTypeId, it->hpMultiplier, room);
                }
                else
                {
                    break;
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

void WaveManager::SpawnMonster(int32_t monsterTypeId, float hpMultiplier, Room *room)
{
    // Random Position around 0,0 (or players)
    // For MVP: Random around 0,0 within 20m
    float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
    float dist = 5.0f + (rand() % 1500) / 100.0f; // 5 to 20
    float x = cos(angle) * dist;
    float y = sin(angle) * dist;

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
