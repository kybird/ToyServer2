#include "Game/Emitter/AoEEmitter.h"

namespace SimpleGame {

AoEEmitter::AoEEmitter(float initialTimer) : _timer(initialTimer)
{
}

void AoEEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
