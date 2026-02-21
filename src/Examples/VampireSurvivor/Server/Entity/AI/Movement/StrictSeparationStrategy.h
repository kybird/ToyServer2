#pragma once

#include "Entity/AI/IMovementStrategy.h"
#include <cmath>
#include <cstdlib>

namespace SimpleGame::Movement {

/**
 * @brief 엄격한 분리(Separation) 이동 전략입니다.
 *
 * 다른 몬스터와 겹치지 않는 것을 최우선으로 합니다.
 * 만약 겹치게 되면, 추적을 멈추고 서로 밀어내는 동작만 수행합니다.
 */
class StrictSeparationStrategy : public SimpleGame::IMovementStrategy
{
public:
    void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;
};

} // namespace SimpleGame::Movement
