#pragma once
#include "IMovementStrategy.h"

namespace SimpleGame {

/**
 * @brief FluidStackingStrategy
 * 몬스터들이 정지하거나 부딪히지 않고, 물처럼 빈 공간을 찾아 흘러 들어가서
 * 플레이어 주변을 차곡차곡 채우는 유체 흐름 방식의 이동 전략입니다.
 */
class FluidStackingStrategy : public IMovementStrategy
{
public:
    FluidStackingStrategy() = default;
    virtual ~FluidStackingStrategy() = default;

    virtual void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;

private:
    float _lastSideStepDir = 0.0f; // 0: none, 1: left, -1: right
};

} // namespace SimpleGame
