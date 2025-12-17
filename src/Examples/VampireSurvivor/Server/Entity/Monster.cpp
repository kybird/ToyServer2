#include "Monster.h"
#include "../Game/Room.h"
#include "AI/IAIBehavior.h"

namespace SimpleGame {

// Static game time tracker for AI Think calls
static float s_gameTime = 0.0f;

void Monster::Update(float dt, Room* room) {
    // Update game time (simplified - should come from Room/Game state ideally)
    s_gameTime += dt;

    if (_ai) {
        // Hybrid AI: Think (periodic) + Execute (every tick)
        _ai->Think(this, room, s_gameTime);
        _ai->Execute(this, dt);
    }
    // If no AI assigned, monster stays idle
}

} // namespace SimpleGame
