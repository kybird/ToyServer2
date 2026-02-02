#pragma once

#include "IMovementStrategy.h"
#include <cmath>
#include <cstdlib>

namespace SimpleGame {

/**
 * @brief Smart flocking strategy.
 *
 * Combines separation, alignment, and look-ahead collision avoidance.
 * Prevents jittering by stopping before collision ("Patience").
 */
class SmartFlockingStrategy : public IMovementStrategy
{
public:
    void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;
};

} // namespace SimpleGame
