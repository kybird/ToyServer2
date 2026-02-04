#pragma once

#include "IMovementStrategy.h"

namespace SimpleGame {

/**
 * @brief Surrounding flocking strategy.
 *
 * Similar to FluidStacking but adds a conscious "encirclement" bias.
 * Monsters are assigned a preferred orbital direction (CW or CCW) based on their ID.
 * When they collide with other monsters, they prefer to slide in their assigned direction,
 * which naturally leads to surrounding the player rather than forming a tight wedge.
 */
class SurroundingFlockingStrategy : public IMovementStrategy
{
public:
    void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) override;
};

} // namespace SimpleGame
