#include "Entity/AI/ChaserAI.h"
#include "Entity/Monster.h"
#include "Game/GameConfig.h"
#include "Game/Room.h"

namespace SimpleGame {

void ChaserAI::Think(Monster *monster, Room *room, float currentTime)
{
    if (currentTime < _nextThinkTime)
        return;
    _nextThinkTime = currentTime + _thinkInterval;

    // Store room pointer for Execute to use
    _room = room;

    // Check if there's any player in the room
    auto target = room->GetNearestPlayer(monster->GetX(), monster->GetY());
    _hasTarget = (target != nullptr);
}

void ChaserAI::Execute(Monster *monster, float dt)
{
    (void)dt;
    float speed = monster->GetSpeed(); // AI now respects dynamic speed from monster
    if (!_hasTarget || !_room)
    {
        monster->SetVelocity(0, 0);
        return;
    }

    // Find nearest player EVERY tick for accurate tracking
    auto target = _room->GetNearestPlayer(monster->GetX(), monster->GetY());
    if (!target)
    {
        monster->SetVelocity(0, 0);
        return;
    }

    // Calculate direction to current player position
    float dx = target->GetX() - monster->GetX();
    float dy = target->GetY() - monster->GetY();
    float distSq = dx * dx + dy * dy;

    if (distSq > 0.001f)
    {
        float dist = std::sqrt(distSq);
        float touchDist = target->GetRadius() + monster->GetRadius();

        // 충돌 방지를 위해 일정 거리(Margin) 밖에서 정지
        // (dist <= touchDist + MARGIN) 일 때 정지
        if (dist <= touchDist + GameConfig::AI_STOP_MARGIN)
        {
            monster->SetVelocity(0, 0);
        }
        else
        {
            // 직선 방향으로만 이동
            float nx = dx / dist;
            float ny = dy / dist;
            monster->SetVelocity(nx * speed, ny * speed);
        }
    }
    else
    {
        monster->SetVelocity(0, 0);
    }
}

void ChaserAI::Reset()
{
    _hasTarget = false;
    _room = nullptr;
    // Load Balancing: Randomize initial think time [0, interval]
    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    _nextThinkTime = r * _thinkInterval;
}

} // namespace SimpleGame
