#include "Monster.h"
#include "AI/IAIBehavior.h"
#include "Game/Room.h"

namespace SimpleGame {

void Monster::Update(float dt, Room *room)
{
    // Update lifetime
    _aliveTime += dt;

    if (_ai)
    {
        // Hybrid AI: Think (periodic) + Execute (every tick)
        _ai->Think(this, room, _aliveTime);
        _ai->Execute(this, dt);
    }
    // If no AI assigned, monster stays idle
}

} // namespace SimpleGame
