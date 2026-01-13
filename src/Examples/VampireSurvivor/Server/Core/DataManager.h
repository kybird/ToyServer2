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
};

struct WaveData
{
    int32_t waveId;
    float startTime;
    float duration;
    int32_t monsterTypeId;
    int32_t count;
    float interval;
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
};

} // namespace SimpleGame
