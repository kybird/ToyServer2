#pragma once

#include "Entity/AI/IMovementStrategy.h"

namespace SimpleGame::Movement {

/**
 * @brief 포위형(Surrounding) 군집 이동 전략입니다.
 *
 * 유체 흐름(FluidStacking) 이동과 유사하지만, 의도적으로 "포위"하려는 특징이 더해집니다.
 * 몬스터 ID를 기준으로 선호하는 회전 방향(시계/반시계)이 부여됩니다.
 * 다른 몬스터와 충돌할 때 할당받은 방향으로 미끄러지려는 성향이 있어,
 * 좁은 쐐기 모양으로 뭉치는 대신 자연스레 플레이어를 포위하는 형태를 유도합니다.
 */
class SurroundingFlockingStrategy : public SimpleGame::IMovementStrategy
{
public:
    void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;
};

} // namespace SimpleGame::Movement
