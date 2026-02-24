#include "Game/Emitter/ArcEmitter.h"

namespace SimpleGame {

ArcEmitter::ArcEmitter(float initialTimer) : _timer(initialTimer)
{
}

void ArcEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
