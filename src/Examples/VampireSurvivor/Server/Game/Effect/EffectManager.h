#pragma once
#include "StatusEffect.h"
#include <memory>
#include <unordered_map>
#include <vector>


namespace SimpleGame {

class Room;
class GameObject;

/**
 * @brief 효과 관리자
 *
 * Room당 하나씩 존재하며, 모든 오브젝트의 효과를 관리
 */
class EffectManager
{
public:
    EffectManager() = default;
    ~EffectManager() = default;

    /**
     * @brief 대상에게 효과 적용
     * @param targetId 대상 오브젝트 ID
     * @param effect 적용할 효과
     */
    void ApplyEffect(int32_t targetId, Effect::StatusEffect effect);

    /**
     * @brief 대상의 특정 타입 효과 제거
     */
    void RemoveEffect(int32_t targetId, Effect::Type type);

    /**
     * @brief 대상의 모든 효과 제거
     */
    void ClearEffects(int32_t targetId);

    /**
     * @brief 매 틱 업데이트 (DoT 처리, 만료 제거)
     * @param currentTime 현재 게임 시간
     * @param room 현재 룸 (데미지 적용용)
     */
    void Update(float currentTime, Room *room);

    /**
     * @brief 이동속도 배율 계산 (SLOW, SPEED_BOOST 반영)
     * @return 1.0 = 100%, 0.5 = 50% 속도
     */
    float GetSpeedMultiplier(int32_t targetId) const;

    /**
     * @brief 공격력 배율 계산 (ATTACK_UP 등 반영)
     */
    float GetAttackMultiplier(int32_t targetId) const;

    /**
     * @brief 대상이 특정 효과를 가지고 있는지 확인
     */
    bool HasEffect(int32_t targetId, Effect::Type type) const;

private:
    // objectId → 활성 효과 목록
    std::unordered_map<int32_t, std::vector<Effect::StatusEffect>> _effects;

    // 만료된 효과 정리
    void CleanupExpired(float currentTime);

    // DoT 데미지 처리
    void ProcessDotDamage(int32_t targetId, Effect::StatusEffect &effect, float currentTime, Room *room);
};

} // namespace SimpleGame
