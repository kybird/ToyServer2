#include "Monster.h"
#include "AI/IAIBehavior.h"
#include "Game/Room.h"

namespace SimpleGame {

void Monster::Update(float dt, Room *room)
{
    // Update lifetime
    _aliveTime += dt;

    if (_ai && !IsDead())
    {
        // Hybrid AI: Think (periodic) + Execute (every tick)
        _ai->Think(this, room, _aliveTime);
        _ai->Execute(this, dt);
    }
    // If no AI assigned, monster stays idle
}

void Monster::TakeDamage(int32_t damage, Room *room)
{
    if (IsDead())
        return;

    _hp -= damage;
    if (_hp <= 0)
    {
        _hp = 0;
        SetState(Protocol::ObjectState::DEAD);
    }
}

} // namespace SimpleGame
