#pragma once

#include "IAIBehavior.h"
#include <cmath>
#include <cstdlib>

namespace SimpleGame {

// Forward declarations
class Monster;
class Room;

/**
 * @brief Wander AI - Random movement pattern.
 * 
 * Think: Picks a new random direction periodically
 * Execute: Moves in current direction
 */
class WanderAI : public IAIBehavior {
public:
    WanderAI(float speed = 1.5f, float directionChangeInterval = 2.0f)
        : _speed(speed), _directionChangeInterval(directionChangeInterval) {}

    void Think(Monster* monster, Room* room, float currentTime) override;
    void Execute(Monster* monster, float dt) override;
    void Reset() override;

private:
    float _speed;
    float _directionChangeInterval;
    float _nextDirectionChangeTime = 0;
    float _dirX = 0;
    float _dirY = 0;
};

} // namespace SimpleGame
