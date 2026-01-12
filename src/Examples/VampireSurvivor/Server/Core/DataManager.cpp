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
        {
            LOG_ERROR("Could not open monster data file: {}", path);
            return false;
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            MonsterTemplate data;
            data.id = item["id"];
            data.name = item["name"];
            data.hp = item["hp"];
            data.speed = item.value("speed", 2.0f);
            data.radius = item.value("radius", 0.5f);
            data.damageOnContact = item.value("damage_on_contact", 10);
            data.attackCooldown = item.value("attack_cooldown", 1.0f);

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
        LOG_ERROR("Failed to load monster data {}: {}", path, e.what());
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

bool DataManager::LoadPlayerData(const std::string &path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR("Could not open player data file: {}", path);
            return false;
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            PlayerTemplate data;
            data.id = item["id"];
            data.name = item["name"];
            data.hp = item["hp"];
            data.speed = item.value("speed", 5.0f);

            if (item.contains("defaultSkills"))
            {
                for (auto &skillId : item["defaultSkills"])
                {
                    data.defaultSkills.push_back(skillId);
                }
            }

            _players[data.id] = data;
        }
        LOG_INFO("Loaded {} players from {}", _players.size(), path);
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Failed to load player data {}: {}", path, e.what());
        return false;
    }
}

const PlayerTemplate *DataManager::GetPlayerTemplate(int32_t id)
{
    auto it = _players.find(id);
    if (it != _players.end())
        return &it->second;
    return nullptr;
}

bool DataManager::LoadSkillData(const std::string &path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR("Could not open skill data file: {}", path);
            return false;
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            SkillTemplate data;
            data.id = item["id"];
            data.name = item["name"];
            data.damage = item["damage"];
            data.tickInterval = item.value("tick_interval", 1.0f);
            data.hitRadius = item.value("hit_radius", 2.0f);
            data.lifeTime = item.value("life_time", 0.0f); // 0 = infinite

            data.emitterType = item.value("emitter_type", "AoE");
            data.typeId = item.value("type_id", 0);
            data.pierce = item.value("pierce", 1);
            data.maxTargetsPerTick = item.value("max_targets_per_tick", 1);
            data.targetRule = item.value("target_rule", "Nearest");

            _skills[data.id] = data;
            LOG_INFO(
                "  - Skill ID: {} | Name: {} | Type: {} | typeId: {}", data.id, data.name, data.emitterType, data.typeId
            );
        }
        LOG_INFO("Loaded {} skills from {}", _skills.size(), path);
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Failed to load skill data {}: {}", path, e.what());
        return false;
    }
}

const SkillTemplate *DataManager::GetSkillTemplate(int32_t id)
{
    auto it = _skills.find(id);
    if (it != _skills.end())
        return &it->second;
    return nullptr;
}

bool DataManager::LoadWaveData(const std::string &path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR("Could not open wave data file: {}", path);
            return false;
        }

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
        LOG_ERROR("Failed to load wave data {}: {}", path, e.what());
        return false;
    }
}

} // namespace SimpleGame
