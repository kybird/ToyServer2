#include "Entity/AI/ChaserAI.h"
#include "Entity/AI/Movement/IMovementStrategy.h"
#include "Entity/Monster.h"
#include "Game/GameConfig.h"
#include "Game/Room.h"
#include "System/Utility/FastRandom.h"

namespace SimpleGame {

void ChaserAI::Think(Monster *monster, Room *room, float currentTime)
{
    if (currentTime < _nextThinkTime)
        return;
    _nextThinkTime = currentTime + _thinkInterval;

    _room = room;

    auto target = room->GetNearestPlayer(monster->GetX(), monster->GetY());
    _hasTarget = (target != nullptr);
}

void ChaserAI::Execute(Monster *monster, float dt)
{
    if (!_hasTarget || _room == nullptr)
    {
        monster->SetVelocity(0, 0);
        return;
    }

    auto target = _room->GetNearestPlayer(monster->GetX(), monster->GetY());
    auto strategy = monster->GetMovementStrategy();

    if (target && strategy)
    {
        float vx = 0, vy = 0;
        strategy->CalculateMovement(monster, _room, dt, target->GetX(), target->GetY(), vx, vy);
        monster->SetVelocity(vx, vy);
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
    static thread_local System::Utility::FastRandom rng;
    _nextThinkTime = rng.NextFloat(0.0f, _thinkInterval);
}

} // namespace SimpleGame
