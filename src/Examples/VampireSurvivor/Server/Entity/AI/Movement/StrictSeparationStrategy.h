#pragma once

#include "IMovementStrategy.h"
#include <cmath>
#include <cstdlib>

namespace SimpleGame {

/**
 * @brief Strict separation strategy.
 *
 * Prioritizes not overlapping with other monsters.
 * If overlapped, it stops chasing and only pushes away.
 */
class StrictSeparationStrategy : public IMovementStrategy
{
public:
    void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;
};

} // namespace SimpleGame
