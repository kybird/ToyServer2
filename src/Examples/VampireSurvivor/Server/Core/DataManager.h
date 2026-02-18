#pragma once
#include "Entity/MonsterAIType.h"
#include "System/ILog.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace SimpleGame {

struct MonsterInfo
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

struct PlayerInfo
{
    int32_t id;
    std::string name;
    int32_t hp;
    float speed;
    std::vector<int32_t> defaultSkills;
};

struct SkillInfo
{
    int32_t id = 0;
    std::string name;
    int32_t damage = 0;
    float tickInterval = 1.0f;
    float hitRadius = 0.5f;
    float lifeTime = 0.0f;

    std::string emitterType = "Linear"; // "Linear", "Orbit", "AoE", "Zone", "Arc"
    int32_t typeId = 0;                 // 클라이언트 프리팹 ID (투사체 등)
    int32_t pierce = 0;                 // 관통 수
    int32_t maxTargetsPerTick = 1;      // 틱당 최대 타겟
    std::string targetRule = "Nearest"; // "Nearest", "Random", "LowestHp"

    // Status Effects
    std::string effectType;      // "POISON", "SLOW", etc. (Empty if none)
    float effectValue = 0.0f;    // Damage for DoT, Scale for Slow (e.g. 0.5)
    float effectDuration = 0.0f; // Duration in seconds
    float effectInterval = 0.0f; // Tick interval for DoT

    // Field/Persistent Skill Stats
    float activeDuration = 0.0f; // How long the field stays active (0 = pulse)
    float dotInterval = 0.5f;    // Tick interval for damage/effect while active
    float arcDegrees = 30.0f;    // [New] Arc angle for Arc emitter type
    float width = 0.0f;          // [New] Rectangular width
    float height = 0.0f;         // [New] Rectangular height

    // Traits (Categories)
    std::vector<std::string> traits; // "PROJECTILE", "AOE", "DURATION", "PIERCE", etc.
};

struct WaveInfo
{
    int32_t waveId;
    float startTime;
    float duration;
    int32_t monsterTypeId;
    int32_t count;
    float interval;
    float hpMultiplier = 1.0f; // Default 1.0
};

struct WeaponLevelInfo
{
    int32_t level;
    int32_t skillId;
    float damageMult = 1.0f;
    float cooldownMult = 1.0f;
    float durationMult = 1.0f;
    float areaMult = 1.0f;
    float speedMult = 1.0f;
    std::string desc;

    // Projectile/Attack modifiers
    int32_t projectileCount = 0;  // Additional projectiles
    int32_t pierceCount = 0;      // Additional pierce
    int32_t maxTargets = 0;       // Override max targets (0 = use base)

    // Critical modifiers
    float critChance = 0.0f;      // Additional crit chance
    float critDamageMult = 1.0f;  // Crit damage multiplier

    // Effect modifiers (for skills with effects)
    std::string effectType;       // Override/add effect type
    float effectValue = 0.0f;
    float effectDuration = 0.0f;

    // Special flags for mechanism changes
    std::vector<std::string> flags; // BIDIRECTIONAL, HOMING, EXPLODE_ON_HIT, etc.

    // Generic overrides for skill parameters (legacy support)
    std::unordered_map<std::string, float> params;
};

struct WeaponInfo
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
    std::vector<WeaponLevelInfo> levels;
};

struct PassiveLevelInfo
{
    int32_t level;
    float bonus = 0.0f;
    float bonus2 = 0.0f;      // Secondary bonus (e.g., speed + crit)
    std::string desc;
};

struct PassiveInfo
{
    int32_t id;
    std::string name;
    std::string description;
    std::string icon;
    std::string statType;      // "damage", "max_hp", "speed", "cooldown", "area", "projectile_count", "pierce", "crit_chance", "crit_damage"
    std::string statType2;     // Secondary stat (optional)
    int32_t maxLevel;
    int32_t weight = 100;      // 가중치 (추첨 확률)
    int32_t uniqueGroup = 0;   // 중복 방지 그룹 ID
    std::vector<PassiveLevelInfo> levels;
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
    const MonsterInfo *GetMonsterInfo(int32_t id);

    bool LoadPlayerData(const std::string &path);
    const PlayerInfo *GetPlayerInfo(int32_t id);

    bool LoadSkillData(const std::string &path);
    const SkillInfo *GetSkillInfo(int32_t id);

    bool LoadWaveData(const std::string &path);
    const std::vector<WaveInfo> &GetWaves() const
    {
        return _waves;
    }

    bool LoadWeaponData(const std::string &path);
    const WeaponInfo *GetWeaponInfo(int32_t id);
    const std::unordered_map<int32_t, WeaponInfo> &GetAllWeapons() const
    {
        return _weapons;
    }

    bool LoadPassiveData(const std::string &path);
    const PassiveInfo *GetPassiveInfo(int32_t id);
    const std::unordered_map<int32_t, PassiveInfo> &GetAllPassives() const
    {
        return _passives;
    }

    // For Testing
    void AddMonsterInfo(const MonsterInfo &tmpl)
    {
        _monsters[tmpl.id] = tmpl;
    }
    void AddPlayerInfo(const PlayerInfo &tmpl)
    {
        _players[tmpl.id] = tmpl;
    }
    void AddSkillInfo(const SkillInfo &tmpl)
    {
        _skills[tmpl.id] = tmpl;
    }

    // For Testing: Inject synthetic weapon data for gtests without modifying JSON files
    void AddWeaponInfoForTest(const WeaponInfo &tmpl)
    {
        _weapons[tmpl.id] = tmpl;
    }

    // For Testing: Clear all weapons to reset between tests and avoid cross-test coupling
    void ClearWeaponsForTest()
    {
        _weapons.clear();
    }

    void ClearPassivesForTest()
    {
        _passives.clear();
    }

private:
    std::unordered_map<int32_t, MonsterInfo> _monsters;
    std::unordered_map<int32_t, PlayerInfo> _players;
    std::unordered_map<int32_t, SkillInfo> _skills;
    std::vector<WaveInfo> _waves;
    std::unordered_map<int32_t, WeaponInfo> _weapons;
    std::unordered_map<int32_t, PassiveInfo> _passives;
};

} // namespace SimpleGame
