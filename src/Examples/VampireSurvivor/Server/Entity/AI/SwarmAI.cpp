#include "Entity/AI/SwarmAI.h"
#include "Entity/Monster.h"
#include "Game/Room.h"

namespace SimpleGame {

void SwarmAI::Think(Monster* monster, Room* room, float currentTime) {
    if (currentTime < _nextThinkTime) return;
    _nextThinkTime = currentTime + _thinkInterval;

    float myX = monster->GetX();
    float myY = monster->GetY();

    // Get player target
    auto player = room->GetNearestPlayer(myX, myY);
    if (player) {
        _playerX = player->GetX();
        _playerY = player->GetY();
        _hasPlayer = true;
    } else {
        _hasPlayer = false;
    }
}

void SwarmAI::Execute(Monster* monster, float dt) {
    (void)dt;
    
    if (!_hasPlayer) {
        monster->SetVelocity(0, 0);
        return;
    }

    float dx = _playerX - monster->GetX();
    float dy = _playerY - monster->GetY();
    float distSq = dx * dx + dy * dy;

    if (distSq > 0.1f) {
        float dist = std::sqrt(distSq);
        float nx = dx / dist;
        float ny = dy / dist;

        // Add slight randomness for swarm effect
        float jitterX = ((rand() % 100) - 50) / 500.0f;
        float jitterY = ((rand() % 100) - 50) / 500.0f;

        monster->SetVelocity((nx + jitterX) * _speed, (ny + jitterY) * _speed);
    } else {
        monster->SetVelocity(0, 0);
    }
}

void SwarmAI::Reset() {
    _hasPlayer = false;
    _playerX = 0;
    _playerY = 0;
    _nextThinkTime = 0;
}

} // namespace SimpleGame
