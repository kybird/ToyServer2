#pragma once
#include <cstdint>

namespace SimpleGame {

/**
 * @brief 스탯 종류 정의
 */
enum class StatType : uint8_t {
    Speed,           // 이동 속도
    Attack,          // 공격력
    MaxHp,           // 최대 체력
    Cooldown,        // 쿨다운 감소
    Area,            // 스킬 범위
    ProjectileCount, // 투사체 수
};

/**
 * @brief Modifier 연산 방식
 */
enum class ModifierOp : uint8_t {
    Flat,        // 고정치 합산: Final = Base + Value
    PercentAdd,  // 퍼센트 합산: Final = Base * (1 + Value)
    PercentMult, // 퍼센트 곱셈: Final = Current * Value
};

/**
 * @brief 스탯 변경 효과를 나타내는 구조체
 *
 * 계산 순서: Flat → PercentAdd → PercentMult
 * 최종 공식: (Base + Σ Flat) × (1.0 + Σ PercentAdd) × Π PercentMult
 */
struct StatModifier
{
    StatType type;        // 적용 스탯
    ModifierOp op;        // 연산 방식
    float value;          // 값 (Flat: 절대값, Percent: 0.1 = 10%)
    int32_t sourceId;     // 소스 식별자 (스킬ID, 오라ID 등)
    float expirationTime; // 만료 시각 (0 = 영구, gameTime 기준)
    bool allowStacking;   // 동일 sourceId 스택 허용 여부

    StatModifier(
        StatType type_, ModifierOp op_, float value_, int32_t sourceId_, float expirationTime_ = 0.0f,
        bool allowStacking_ = false
    )
        : type(type_), op(op_), value(value_), sourceId(sourceId_), expirationTime(expirationTime_),
          allowStacking(allowStacking_)
    {
    }
};

} // namespace SimpleGame
