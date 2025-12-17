#include "Core/DataManager.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>


namespace SimpleGame {

bool DataManager::LoadMonsterData(const std::string &path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
            return false;

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            MonsterTemplate data;
            data.id = item["id"];
            data.name = item["name"];
            data.hp = item["hp"];
            data.speed = item.value("speed", 2.0f);

            std::string ai = item.value("ai_type", "CHASER");
            if (ai == "SWARM")
                data.aiType = MonsterAIType::SWARM;
            else if (ai == "WANDER")
                data.aiType = MonsterAIType::WANDER;
            else
                data.aiType = MonsterAIType::CHASER;

            _monsters[data.id] = data;
        }
        LOG_INFO("Loaded {} monsters from {}", _monsters.size(), path);
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Failed to load monster data: {}", e.what());
        return false;
    }
}

const MonsterTemplate *DataManager::GetMonsterTemplate(int32_t id)
{
    auto it = _monsters.find(id);
    if (it != _monsters.end())
        return &it->second;
    return nullptr;
}

bool DataManager::LoadWaveData(const std::string &path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
            return false;

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            WaveData data;
            data.waveId = item["wave_id"];
            data.startTime = item["start_time"];
            data.duration = item["duration"]; // Not always used if fixed pattern
            data.monsterTypeId = item["monster_type_id"];
            data.count = item["count"];
            data.interval = item.value("interval", 1.0f);

            _waves.push_back(data);
        }
        std::sort(
            _waves.begin(),
            _waves.end(),
            [](const WaveData &a, const WaveData &b)
            {
                return a.startTime < b.startTime;
            }
        );

        LOG_INFO("Loaded {} wave entries from {}", _waves.size(), path);
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Failed to load wave data: {}", e.what());
        return false;
    }
}

} // namespace SimpleGame
