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
    MonsterAIType aiType;
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

private:
    std::unordered_map<int32_t, MonsterTemplate> _monsters;
    std::vector<WaveData> _waves;
};

} // namespace SimpleGame
