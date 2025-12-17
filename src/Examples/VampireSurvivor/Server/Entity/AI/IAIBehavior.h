#pragma once

namespace SimpleGame {

class Monster;
class Room;

/**
 * @brief Hybrid AI Behavior interface.
 * 
 * Think(): Called periodically (e.g., every 0.5s) for decision making.
 * Execute(): Called every tick for action execution (movement, etc).
 */
class IAIBehavior {
public:
    virtual ~IAIBehavior() = default;

    /**
     * @brief Make decisions (target selection, state changes).
     * Called less frequently to save CPU.
     * @param monster The monster this AI controls
     * @param room Current room for world queries
     * @param currentTime Game time in seconds
     */
    virtual void Think(Monster* monster, Room* room, float currentTime) = 0;

    /**
     * @brief Execute current behavior (movement, attacks).
     * Called every tick.
     * @param monster The monster this AI controls
     * @param dt Delta time in seconds
     */
    virtual void Execute(Monster* monster, float dt) = 0;

    /**
     * @brief Reset AI state (when monster is recycled from pool).
     */
    virtual void Reset() = 0;
};

} // namespace SimpleGame
