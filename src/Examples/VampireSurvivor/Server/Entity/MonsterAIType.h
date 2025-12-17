#pragma once

namespace SimpleGame {

/**
 * @brief Monster AI type enum for data loading and factory creation.
 */
enum class MonsterAIType {
    CHASER,   // Pursues nearest player
    WANDER,   // Random movement
    SWARM,    // Boids-like flocking
    BOSS      // Behavior tree based
};

} // namespace SimpleGame
