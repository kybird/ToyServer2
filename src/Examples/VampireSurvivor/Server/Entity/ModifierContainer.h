#pragma once
#include "StatModifier.h"
#include <unordered_map>
#include <vector>

namespace SimpleGame {

/**
 * @brief 스탯 Modifier 관리 및 계산 컨테이너
 *
 * 계산 공식: (Base + Σ Flat) × (1.0 + Σ PercentAdd) × Π PercentMult
 *
 * 중첩 정책:
 * - Refresh: 동일 sourceId, allowStacking=false → 기존 제거 후 갱신
 * - Stack: 동일 sourceId, allowStacking=true → 누적
 * - Add: 다른 sourceId → 항상 추가
 */
class ModifierContainer
{
public:
    ModifierContainer();

    // Base 스탯 설정
    void SetBaseStat(StatType type, float value);
    float GetBaseStat(StatType type) const;

    // Modifier 추가/제거
    void AddModifier(const StatModifier &mod);
    void RemoveBySourceId(int32_t sourceId);
    void RemoveBySourceIdAndType(int32_t sourceId, StatType type);
    void Clear();

    // 최종 스탯 계산 (캐싱 적용)
    float GetStat(StatType type);

    // 시간 기반 만료 처리
    void Update(float currentGameTime);

private:
    std::unordered_map<StatType, float> _baseStats;
    std::unordered_map<StatType, float> _cachedStats;
    std::unordered_map<StatType, bool> _dirty;
    std::vector<StatModifier> _modifiers;

    float CalculateStat(StatType type);
    void SetDirty(StatType type);
};

} // namespace SimpleGame
