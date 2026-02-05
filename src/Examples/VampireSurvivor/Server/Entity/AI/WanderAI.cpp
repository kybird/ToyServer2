#include "Entity/AI/WanderAI.h"
#include "Entity/Monster.h"
#include "System/Utility/FastRandom.h"

namespace SimpleGame {

void WanderAI::Think(Monster *monster, Room *room, float currentTime)
{
    (void)monster;
    (void)room;

    if (currentTime < _nextDirectionChangeTime)
        return;
    _nextDirectionChangeTime = currentTime + _directionChangeInterval;

    // Pick random direction
    static thread_local System::Utility::FastRandom rng;
    float angle = rng.NextFloat(0.0f, 2.0f * 3.14159265f);
    _dirX = std::cos(angle);
    _dirY = std::sin(angle);
}

void WanderAI::Execute(Monster *monster, float dt)
{
    (void)dt;
    float speed = monster->GetSpeed(); // AI now respects dynamic speed from monster
    monster->SetVelocity(_dirX * speed, _dirY * speed);
}

void WanderAI::Reset()
{
    _dirX = 0;
    _dirY = 0;
    _nextDirectionChangeTime = 0;
}

} // namespace SimpleGame
