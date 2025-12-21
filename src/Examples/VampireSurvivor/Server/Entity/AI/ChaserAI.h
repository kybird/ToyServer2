#pragma once

#include "IAIBehavior.h"
#include <cmath>

namespace SimpleGame {

// Forward declarations
class Monster;
class Room;

/**
 * @brief Chaser AI - Pursues nearest player.
 *
 * Think: Finds nearest player target (every 0.5s)
 * Execute: Moves toward current target
 */
class ChaserAI : public IAIBehavior
{
public:
    ChaserAI(float speed = 2.0f, float thinkInterval = 0.1f) : _speed(speed), _thinkInterval(thinkInterval)
    {
    }

    void Think(Monster *monster, Room *room, float currentTime) override;
    void Execute(Monster *monster, float dt) override;
    void Reset() override;

private:
    float _speed;
    float _thinkInterval;
    float _nextThinkTime = 0;
    bool _hasTarget = false;
    float _targetX = 0;
    float _targetY = 0;
};

} // namespace SimpleGame
