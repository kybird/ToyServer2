#include "Monster.h"
#include "AI/IAIBehavior.h"
#include "Game/Room.h"

namespace SimpleGame {

void Monster::Update(float dt, Room *room)
{
    // Update lifetime
    _aliveTime += dt;

    // Update state expiry
    UpdateStateExpiry(_aliveTime);

    if (_ai != nullptr && !IsDead())
    {
        // Skip AI if control is disabled (e.g. Knockback, Stun)
        if (IsControlDisabled())
        {
            return;
        }

        // Hybrid AI: Think (periodic) + Execute (every tick)
        _ai->Think(this, room, _aliveTime);
        _ai->Execute(this, dt);

        // Speed multiplier is now naturally handled by AI using GetSpeed() inside Execute()
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
