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
            std::exit(EXIT_FAILURE);
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            MonsterInfo data;
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
        std::exit(EXIT_FAILURE);
    }
}

const MonsterInfo *DataManager::GetMonsterInfo(int32_t id)
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
            std::exit(EXIT_FAILURE);
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            PlayerInfo data;
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
        std::exit(EXIT_FAILURE);
    }
}

const PlayerInfo *DataManager::GetPlayerInfo(int32_t id)
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
            std::exit(EXIT_FAILURE);
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            SkillInfo data;
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

            // Effect Parsing
            data.effectType = item.value("effect_type", "");
            data.effectValue = item.value("effect_value", 0.0f);
            data.effectDuration = item.value("effect_duration", 0.0f);
            data.effectInterval = item.value("effect_interval", 0.0f);

            // Field Stats
            data.activeDuration = item.value("active_duration", 0.0f);
            data.dotInterval = item.value("dot_interval", 0.5f);
            data.arcDegrees = item.value("arc_degrees", 30.0f); // [New] Arc angle for Arc emitter type
            data.width = item.value("width", 0.0f);             // [New] Initial width
            data.height = item.value("height", 0.0f);           // [New] Initial height

            // Traits Parsing
            if (item.contains("traits") && item["traits"].is_array())
            {
                for (const auto &trait : item["traits"])
                {
                    data.traits.push_back(trait.get<std::string>());
                }
            }

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
        std::exit(EXIT_FAILURE);
    }
}

const SkillInfo *DataManager::GetSkillInfo(int32_t id)
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
            std::exit(EXIT_FAILURE);
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            WaveInfo data;
            data.waveId = item["wave_id"];
            data.startTime = item["start_time"];
            data.duration = item["duration"]; // Not always used if fixed pattern
            data.monsterTypeId = item["monster_type_id"];
            data.count = item["count"];
            data.interval = item.value("interval", 1.0f);
            data.hpMultiplier = item.value("hp_multiplier", 1.0f);

            _waves.push_back(data);
        }
        std::sort(
            _waves.begin(),
            _waves.end(),
            [](const WaveInfo &a, const WaveInfo &b)
            {
                return a.startTime < b.startTime;
            }
        );

        LOG_INFO("Loaded {} wave entries from {}", _waves.size(), path);
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Failed to load wave data {}: {}", path, e.what());
        std::exit(EXIT_FAILURE);
    }
}

bool DataManager::LoadWeaponData(const std::string &path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR("Could not open weapon data file: {}", path);
            std::exit(EXIT_FAILURE);
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            WeaponInfo data;
            data.id = item["id"];
            data.name = item["name"];
            data.description = item.value("description", "");
            data.icon = item.value("icon", "");
            data.maxLevel = item.value("max_level", 8);
            data.weight = item.value("weight", 100);
            data.uniqueGroup = item.value("unique_group", 0);
            data.evolutionId = item.value("evolution_id", 0);
            data.evolutionPassiveId = item.value("evolution_passive_id", 0);

            if (item.contains("levels"))
            {
                for (const auto &lvl : item["levels"])
                {
                    WeaponLevelInfo levelData;
                    levelData.level = lvl["level"];
                    levelData.skillId = lvl["skill_id"];
                    levelData.damageMult = lvl.value("damage_mult", 1.0f);
                    levelData.cooldownMult = lvl.value("cooldown_mult", 1.0f);
                    levelData.durationMult = lvl.value("duration_mult", 1.0f);
                    levelData.areaMult = lvl.value("area_mult", 1.0f);
                    levelData.speedMult = lvl.value("speed_mult", 1.0f);
                    levelData.desc = lvl.value("desc", "");

                    levelData.projectileCount = lvl.value("projectile_count", 0);
                    levelData.pierceCount = lvl.value("pierce_count", 0);
                    levelData.maxTargets = lvl.value("max_targets", 0);

                    levelData.critChance = lvl.value("crit_chance", 0.0f);
                    levelData.critDamageMult = lvl.value("crit_damage_mult", 1.0f);

                    levelData.effectType = lvl.value("effect_type", "");
                    levelData.effectValue = lvl.value("effect_value", 0.0f);
                    levelData.effectDuration = lvl.value("effect_duration", 0.0f);

                    if (lvl.contains("flags") && lvl["flags"].is_array())
                    {
                        for (const auto &flag : lvl["flags"])
                        {
                            levelData.flags.push_back(flag.get<std::string>());
                        }
                    }

                    if (lvl.contains("params") && lvl["params"].is_object())
                    {
                        for (auto it = lvl["params"].begin(); it != lvl["params"].end(); ++it)
                        {
                            if (it.value().is_number())
                            {
                                levelData.params[it.key()] = it.value().get<float>();
                            }
                        }
                    }

                    data.levels.push_back(levelData);
                }
            }

            _weapons[data.id] = data;
        }
        LOG_INFO("Loaded {} weapons from {}", _weapons.size(), path);
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Failed to load weapon data {}: {}", path, e.what());
        std::exit(EXIT_FAILURE);
    }
}

const WeaponInfo *DataManager::GetWeaponInfo(int32_t id)
{
    auto it = _weapons.find(id);
    if (it != _weapons.end())
        return &it->second;
    return nullptr;
}

bool DataManager::LoadPassiveData(const std::string &path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR("Could not open passive data file: {}", path);
            std::exit(EXIT_FAILURE);
        }

        nlohmann::json j;
        file >> j;

        for (const auto &item : j)
        {
            PassiveInfo data;
            data.id = item["id"];
            data.name = item["name"];
            data.description = item.value("description", "");
            data.icon = item.value("icon", "");
            data.statType = item.value("stat_type", "");
            data.statType2 = item.value("stat_type2", "");
            data.maxLevel = item.value("max_level", 5);
            data.weight = item.value("weight", 100);
            data.uniqueGroup = item.value("unique_group", 0);

            if (item.contains("levels"))
            {
                for (const auto &lvl : item["levels"])
                {
                    PassiveLevelInfo levelData;
                    levelData.level = lvl["level"];
                    levelData.bonus = lvl.value("bonus", 0.0f);
                    levelData.bonus2 = lvl.value("bonus2", 0.0f);
                    levelData.desc = lvl.value("desc", "");
                    data.levels.push_back(levelData);
                }
            }

            _passives[data.id] = data;
        }
        LOG_INFO("Loaded {} passives from {}", _passives.size(), path);
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Failed to load passive data {}: {}", path, e.what());
        std::exit(EXIT_FAILURE);
    }
}

const PassiveInfo *DataManager::GetPassiveInfo(int32_t id)
{
    auto it = _passives.find(id);
    if (it != _passives.end())
        return &it->second;
    return nullptr;
}

bool DataManager::LoadMapData(int32_t mapId, const std::string &path)
{
    auto tileMap = std::make_unique<TileMap>();
    if (!tileMap->LoadFromJson(path))
    {
        LOG_ERROR("DataManager: Failed to load TileMap (ID:{}) from {}", mapId, path);
        return false;
    }

    _maps[mapId] = std::move(tileMap);
    LOG_INFO("DataManager: Successfully loaded TileMap (ID:{})", mapId);
    return true;
}

const TileMap *DataManager::GetMap(int32_t mapId) const
{
    auto it = _maps.find(mapId);
    if (it != _maps.end())
        return it->second.get();
    return nullptr;
}

} // namespace SimpleGame
