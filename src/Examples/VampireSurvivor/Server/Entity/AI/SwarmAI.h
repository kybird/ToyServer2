#pragma once

#include "Entity/AI/IMovementStrategy.h"
#include "Entity/Monster.h" // [Fix] Include for GetMovementStrategy
#include "IAIBehavior.h"
#include <cmath>
#include <cstdlib>

namespace SimpleGame {

// Forward declarations
// class Monster;
class Room;

/**
 * @brief Swarm AI - Boids-like flocking behavior.
 *
 * Combines chase with slight randomization for swarm effect.
 */
class SwarmAI : public IAIBehavior
{
public:
    SwarmAI(float speed = 2.5f, float thinkInterval = 0.3f) : _speed(speed), _thinkInterval(thinkInterval)
    {
    }

    void Think(Monster *monster, Room *room, float currentTime) override;
    void Execute(Monster *monster, float dt) override;
    void Reset() override;

private:
    float _speed;
    float _thinkInterval;
    float _nextThinkTime = 0;
    bool _hasPlayer = false;
    float _playerX = 0;
    float _playerY = 0;
    Room *_room = nullptr; // [New] Store room reference
};

} // namespace SimpleGame
