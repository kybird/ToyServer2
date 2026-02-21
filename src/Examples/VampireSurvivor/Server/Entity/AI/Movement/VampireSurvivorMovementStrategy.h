#pragma once

#include "../IMovementStrategy.h"

namespace SimpleGame {

/**
 * @brief 뱀파이어 서바이벌 특유의 몬스터 이동 전략입니다.
 *
 * 플레이어를 향해 끊임없이 직진하며, 다른 몬스터들과 겹칠 경우
 * 부드럽게 서로 밀어내는(Soft Collision) 로직을 적용합니다.
 */
class VampireSurvivorMovementStrategy : public IMovementStrategy
{
public:
    VampireSurvivorMovementStrategy() = default;
    virtual ~VampireSurvivorMovementStrategy() override = default;

    virtual void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;
};

} // namespace SimpleGame
