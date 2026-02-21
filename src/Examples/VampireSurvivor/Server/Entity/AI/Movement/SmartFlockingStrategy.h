#pragma once

#include "Entity/AI/IMovementStrategy.h"
#include <cmath>
#include <cstdlib>

namespace SimpleGame::Movement {

/**
 * @brief 스마트 군집(Flocking) 이동 전략입니다.
 *
 * 분리, 정렬 기능에 전방 주시 충돌 회피 기법을 결합합니다.
 * 충돌 전에 정지하여 부들거리는 현상(Jittering)을 방지합니다("인내심" 로직 적용).
 */
class SmartFlockingStrategy : public SimpleGame::IMovementStrategy
{
public:
    void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;
};

} // namespace SimpleGame::Movement
