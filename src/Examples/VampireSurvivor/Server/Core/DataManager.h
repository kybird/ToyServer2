#pragma once
#include "Entity/MonsterAIType.h"
#include "System/ILog.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace SimpleGame {

struct MonsterTemplate
{
    int32_t id;
    std::string name;
    int32_t hp;
    float speed;
    float radius;
    int32_t damageOnContact;
    float attackCooldown;
    MonsterAIType aiType;
};

struct PlayerTemplate
{
    int32_t id;
    std::string name;
    int32_t hp;
    float speed;
    std::vector<int32_t> defaultSkills;
};

struct SkillTemplate
{
    int32_t id;
    std::string name;
    int32_t damage;
    float tickInterval;
    float hitRadius;
    float lifeTime;

    std::string emitterType;   // "Linear", "Orbit", "AoE"
    int32_t typeId;            // 클라이언트 프리팹 ID (투사체 등)
    int32_t pierce;            // 관통 수
    int32_t maxTargetsPerTick; // 틱당 최대 타겟
    std::string targetRule;    // "Nearest", "Random", "LowestHp"

    // Status Effects
    std::string effectType; // "POISON", "SLOW", etc. (Empty if none)
    float effectValue;      // Damage for DoT, Scale for Slow (e.g. 0.5)
    float effectDuration;   // Duration in seconds
    float effectInterval;   // Tick interval for DoT

    // Field/Persistent Skill Stats
    float activeDuration;     // How long the field stays active (0 = pulse)
    float dotInterval;        // Tick interval for damage/effect while active
    float arcDegrees = 30.0f; // [New] Arc angle for Arc emitter type
    float width = 0.0f;       // [New] Rectangular width
    float height = 0.0f;      // [New] Rectangular height

    // Traits (Categories)
    std::vector<std::string> traits; // "PROJECTILE", "AOE", "DURATION", "PIERCE", etc.
};

struct WaveData
{
    int32_t waveId;
    float startTime;
    float duration;
    int32_t monsterTypeId;
    int32_t count;
    float interval;
    float hpMultiplier = 1.0f; // Default 1.0
};

struct WeaponLevelData
{
    int32_t level;
    int32_t skillId;
    float damageMult = 1.0f;
    float cooldownMult = 1.0f;
    float durationMult = 1.0f; // New field for scaling active duration
    std::string desc;

    // [New] Generic overrides for skill parameters
    std::unordered_map<std::string, float> params;
};

struct WeaponTemplate
{
    int32_t id;
    std::string name;
    std::string description;
    std::string icon;
    int32_t maxLevel;
    int32_t weight = 100;           // 가중치 (추첨 확률)
    int32_t uniqueGroup = 0;        // 중복 방지 그룹 ID
    int32_t evolutionId = 0;        // 진화 결과 무기 ID
    int32_t evolutionPassiveId = 0; // 진화에 필요한 패시브 ID
    std::vector<WeaponLevelData> levels;
};

struct PassiveLevelData
{
    int32_t level;
    float bonus;
    std::string desc;
};

struct PassiveTemplate
{
    int32_t id;
    std::string name;
    std::string description;
    std::string icon;
    std::string statType; // "damage", "max_hp", "speed", "cooldown", "area", "projectile_count"
    int32_t maxLevel;
    int32_t weight = 100;    // 가중치 (추첨 확률)
    int32_t uniqueGroup = 0; // 중복 방지 그룹 ID
    std::vector<PassiveLevelData> levels;
};

class DataManager
{
public:
    static DataManager &Instance()
    {
        static DataManager instance;
        return instance;
    }

    bool LoadMonsterData(const std::string &path);
    const MonsterTemplate *GetMonsterTemplate(int32_t id);

    bool LoadPlayerData(const std::string &path);
    const PlayerTemplate *GetPlayerTemplate(int32_t id);

    bool LoadSkillData(const std::string &path);
    const SkillTemplate *GetSkillTemplate(int32_t id);

    bool LoadWaveData(const std::string &path);
    const std::vector<WaveData> &GetWaves() const
    {
        return _waves;
    }

    bool LoadWeaponData(const std::string &path);
    const WeaponTemplate *GetWeaponTemplate(int32_t id);
    const std::unordered_map<int32_t, WeaponTemplate> &GetAllWeapons() const
    {
        return _weapons;
    }

    bool LoadPassiveData(const std::string &path);
    const PassiveTemplate *GetPassiveTemplate(int32_t id);
    const std::unordered_map<int32_t, PassiveTemplate> &GetAllPassives() const
    {
        return _passives;
    }

    // For Testing
    void AddMonsterTemplate(const MonsterTemplate &tmpl)
    {
        _monsters[tmpl.id] = tmpl;
    }
    void AddPlayerTemplate(const PlayerTemplate &tmpl)
    {
        _players[tmpl.id] = tmpl;
    }
    void AddSkillTemplate(const SkillTemplate &tmpl)
    {
        _skills[tmpl.id] = tmpl;
    }

private:
    std::unordered_map<int32_t, MonsterTemplate> _monsters;
    std::unordered_map<int32_t, PlayerTemplate> _players;
    std::unordered_map<int32_t, SkillTemplate> _skills;
    std::vector<WaveData> _waves;
    std::unordered_map<int32_t, WeaponTemplate> _weapons;
    std::unordered_map<int32_t, PassiveTemplate> _passives;
};

} // namespace SimpleGame
