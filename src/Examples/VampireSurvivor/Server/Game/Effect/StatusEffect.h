#pragma once
#include <cstdint>

namespace SimpleGame {
namespace Effect {

/**
 * @brief 효과 타입 정의
 *
 * State(상태)와 달리 중첩 가능한 지속 효과들
 */
enum class Type {
    POISON,       // 독 - 주기적 데미지
    BURN,         // 화상 - 주기적 데미지
    SLOW,         // 슬로우 - 이동속도 감소 (value = 0.5면 50% 감소)
    SPEED_BOOST,  // 가속 - 이동속도 증가
    ATTACK_UP,    // 공격력 증가
    DEFENSE_DOWN, // 방어력 감소
};

/**
 * @brief 상태 효과 구조체
 */
struct StatusEffect
{
    Type type;                 // 효과 종류
    int32_t sourceId = 0;      // 효과를 건 대상 ID (킬 카운트 등에 사용)
    float endTime = 0.0f;      // 만료 시간 (게임 시간 기준)
    float tickInterval = 0.0f; // DoT 틱 간격 (0이면 틱 없음)
    float lastTickTime = 0.0f; // 마지막 틱 처리 시간
    float value = 0.0f;        // 효과 수치 (데미지량, 슬로우 비율 등)

    // 만료 체크
    bool IsExpired(float currentTime) const
    {
        return currentTime >= endTime;
    }

    // DoT 틱이 필요한지 체크
    bool NeedsTick(float currentTime) const
    {
        return tickInterval > 0.0f && (currentTime - lastTickTime) >= tickInterval;
    }
};

} // namespace Effect
} // namespace SimpleGame
