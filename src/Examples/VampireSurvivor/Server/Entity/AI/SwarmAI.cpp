#include "Entity/AI/SwarmAI.h"
#include "Entity/AI/Movement/IMovementStrategy.h"
#include "Entity/Monster.h"
#include "Game/GameConfig.h"
#include "Game/Room.h"

namespace SimpleGame {

void SwarmAI::Think(Monster *monster, Room *room, float currentTime)
{
    if (currentTime < _nextThinkTime)
        return;
    _nextThinkTime = currentTime + _thinkInterval;

    _room = room;
    auto player = room->GetNearestPlayer(monster->GetX(), monster->GetY());

    if (player)
    {
        _playerX = player->GetX();
        _playerY = player->GetY();
        _hasPlayer = true;
    }
    else
    {
        _hasPlayer = false;
    }
}

void SwarmAI::Execute(Monster *monster, float dt)
{
    if (!_hasPlayer || _room == nullptr)
    {
        monster->SetVelocity(0, 0);
        return;
    }

    auto strategy = monster->GetMovementStrategy();
    if (strategy)
    {
        float vx = 0, vy = 0;
        strategy->CalculateMovement(monster, _room, dt, _playerX, _playerY, vx, vy);
        monster->SetVelocity(vx, vy);
    }
    else
    {
        monster->SetVelocity(0, 0);
    }
}

void SwarmAI::Reset()
{
    _hasPlayer = false;
    _playerX = 0;
    _playerY = 0;
    _nextThinkTime = 0;
    _room = nullptr;
}

} // namespace SimpleGame
