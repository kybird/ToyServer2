#include "Entity/AI/SwarmAI.h"
#include "Entity/Monster.h"
#include "Game/GameConfig.h"
#include "Game/Room.h"

namespace SimpleGame {

void SwarmAI::Think(Monster *monster, Room *room, float currentTime)
{
    if (currentTime < _nextThinkTime)
        return;
    _nextThinkTime = currentTime + _thinkInterval;

    float myX = monster->GetX();
    float myY = monster->GetY();

    // Get player target
    auto player = room->GetNearestPlayer(myX, myY);
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
    (void)dt;

    if (!_hasPlayer)
    {
        monster->SetVelocity(0, 0);
        return;
    }

    float dx = _playerX - monster->GetX();
    float dy = _playerY - monster->GetY();
    float distSq = dx * dx + dy * dy;

    if (distSq > 0.1f)
    {
        float dist = std::sqrt(distSq);

        // 충돌 방지를 위해 일정 거리(Margin) 밖에서 정지
        // 플레이어 반경 0.5f (GameConfig::PLAYER_COLLISION_RADIUS) 가정
        float touchDist = GameConfig::PLAYER_COLLISION_RADIUS + monster->GetRadius();

        if (dist <= touchDist + GameConfig::AI_STOP_MARGIN)
        {
            monster->SetVelocity(0, 0);
        }
        else
        {
            float nx = dx / dist;
            float ny = dy / dist;

            // Add slight randomness for swarm effect
            float jitterX = ((rand() % 100) - 50) / 500.0f;
            float jitterY = ((rand() % 100) - 50) / 500.0f;

            float speed = monster->GetSpeed(); // Use dynamic speed from monster
            monster->SetVelocity((nx + jitterX) * speed, (ny + jitterY) * speed);
        }
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
}

} // namespace SimpleGame
