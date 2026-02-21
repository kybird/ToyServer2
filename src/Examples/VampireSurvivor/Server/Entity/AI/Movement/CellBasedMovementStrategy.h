#pragma once
#include "Entity/AI/IMovementStrategy.h"

namespace SimpleGame::Movement {

class CellBasedMovementStrategy : public SimpleGame::IMovementStrategy
{
public:
    CellBasedMovementStrategy() = default;
    virtual ~CellBasedMovementStrategy() = default;

    void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;
};

} // namespace SimpleGame::Movement
