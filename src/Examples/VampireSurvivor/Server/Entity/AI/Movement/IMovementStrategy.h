#pragma once

namespace SimpleGame {

class Monster;
class Room;

/**
 * @brief Interface for monster movement logic.
 *
 * Allows swapping different steering behaviors (Separation, Flocking, Pathfinding)
 * without modifying the core AI classes.
 */
class IMovementStrategy
{
public:
    virtual ~IMovementStrategy() = default;

    /**
     * @brief Calculates the next velocity for the monster.
     *
     * @param monster The monster entity.
     * @param room The room (for spatial queries).
     * @param dt Delta time.
     * @param targetX The X coordinate of the target (e.g. player).
     * @param targetY The Y coordinate of the target.
     * @param outVx Output velocity X.
     * @param outVy Output velocity Y.
     */
    virtual void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) = 0;
};

} // namespace SimpleGame
