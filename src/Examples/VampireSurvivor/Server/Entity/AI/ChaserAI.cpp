#include "Entity/AI/ChaserAI.h"
#include "Entity/Monster.h"
#include "Game/Room.h"

namespace SimpleGame {

void ChaserAI::Think(Monster* monster, Room* room, float currentTime) {
    if (currentTime < _nextThinkTime) return;
    _nextThinkTime = currentTime + _thinkInterval;

    // Find nearest player
    auto target = room->GetNearestPlayer(monster->GetX(), monster->GetY());
    if (target) {
        _targetX = target->GetX();
        _targetY = target->GetY();
        _hasTarget = true;
    } else {
        _hasTarget = false;
    }
}

void ChaserAI::Execute(Monster* monster, float dt) {
    (void)dt;
    if (!_hasTarget) {
        monster->SetVelocity(0, 0);
        return;
    }

    float dx = _targetX - monster->GetX();
    float dy = _targetY - monster->GetY();
    float distSq = dx * dx + dy * dy;

    if (distSq > 0.1f) {
        float dist = std::sqrt(distSq);
        float nx = dx / dist;
        float ny = dy / dist;
        monster->SetVelocity(nx * _speed, ny * _speed);
    } else {
        monster->SetVelocity(0, 0);
    }
}

void ChaserAI::Reset() {
    _hasTarget = false;
    _targetX = 0;
    _targetY = 0;
    _nextThinkTime = 0;
}

} // namespace SimpleGame
