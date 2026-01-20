#include "ModifierContainer.h"
#include <algorithm>
#include <cmath>

namespace SimpleGame {

ModifierContainer::ModifierContainer()
{
    // 초기 capacity 설정 (대부분 0~2개)
    _modifiers.reserve(2);
}

void ModifierContainer::SetBaseStat(StatType type, float value)
{
    _baseStats[type] = value;
    SetDirty(type);
}

float ModifierContainer::GetBaseStat(StatType type) const
{
    auto it = _baseStats.find(type);
    return (it != _baseStats.end()) ? it->second : 0.0f;
}

void ModifierContainer::AddModifier(const StatModifier &mod)
{
    // 중첩 정책: Refresh (동일 sourceId, allowStacking=false)
    if (!mod.allowStacking)
    {
        // 동일 sourceId + 동일 type의 기존 Modifier 제거
        _modifiers.erase(
            std::remove_if(
                _modifiers.begin(),
                _modifiers.end(),
                [&](const StatModifier &m)
                {
                    return m.sourceId == mod.sourceId && m.type == mod.type && !m.allowStacking;
                }
            ),
            _modifiers.end()
        );
    }

    _modifiers.push_back(mod);
    SetDirty(mod.type);
}

void ModifierContainer::RemoveBySourceId(int32_t sourceId)
{
    auto it = std::remove_if(
        _modifiers.begin(),
        _modifiers.end(),
        [sourceId](const StatModifier &m)
        {
            return m.sourceId == sourceId;
        }
    );

    // Dirty 설정
    for (auto iter = it; iter != _modifiers.end(); ++iter)
    {
        SetDirty(iter->type);
    }

    _modifiers.erase(it, _modifiers.end());
}

void ModifierContainer::RemoveBySourceIdAndType(int32_t sourceId, StatType type)
{
    auto it = std::remove_if(
        _modifiers.begin(),
        _modifiers.end(),
        [sourceId, type](const StatModifier &m)
        {
            return m.sourceId == sourceId && m.type == type;
        }
    );

    if (it != _modifiers.end())
    {
        SetDirty(type);
        _modifiers.erase(it, _modifiers.end());
    }
}

void ModifierContainer::Clear()
{
    _modifiers.clear();
    _cachedStats.clear();
    _dirty.clear();
}

float ModifierContainer::GetStat(StatType type)
{
    // Dirty Flag 확인
    auto dirtyIt = _dirty.find(type);
    if (dirtyIt != _dirty.end() && dirtyIt->second)
    {
        _cachedStats[type] = CalculateStat(type);
        _dirty[type] = false;
    }
    else if (_cachedStats.find(type) == _cachedStats.end())
    {
        // 캐시가 없으면 계산
        _cachedStats[type] = CalculateStat(type);
    }

    return _cachedStats[type];
}

void ModifierContainer::Update(float currentGameTime)
{
    auto it = std::remove_if(
        _modifiers.begin(),
        _modifiers.end(),
        [currentGameTime](const StatModifier &m)
        {
            // expirationTime이 0이면 영구 효과
            return m.expirationTime > 0.0f && currentGameTime >= m.expirationTime;
        }
    );

    // 만료된 Modifier의 타입을 Dirty 설정
    for (auto iter = it; iter != _modifiers.end(); ++iter)
    {
        SetDirty(iter->type);
    }

    _modifiers.erase(it, _modifiers.end());
}

float ModifierContainer::CalculateStat(StatType type)
{
    float base = GetBaseStat(type);
    float flatSum = 0.0f;
    float percentAddSum = 0.0f;
    float percentMultProduct = 1.0f;

    // Modifier 분류 및 합산
    for (const auto &mod : _modifiers)
    {
        if (mod.type != type)
            continue;

        switch (mod.op)
        {
        case ModifierOp::Flat:
            flatSum += mod.value;
            break;
        case ModifierOp::PercentAdd:
            percentAddSum += mod.value;
            break;
        case ModifierOp::PercentMult:
            percentMultProduct *= mod.value;
            break;
        }
    }

    // 계산 공식: (Base + Flat) × (1 + PercentAdd) × PercentMult
    float result = (base + flatSum) * (1.0f + percentAddSum) * percentMultProduct;

    // 부동소수점 오차 방지 (소수점 3자리 반올림)
    result = std::round(result * 1000.0f) / 1000.0f;

    // 최소값 클램핑 (Speed는 최소 0.1)
    if (type == StatType::Speed && result < 0.1f)
    {
        result = 0.1f;
    }

    return result;
}

void ModifierContainer::SetDirty(StatType type)
{
    _dirty[type] = true;
}

} // namespace SimpleGame
