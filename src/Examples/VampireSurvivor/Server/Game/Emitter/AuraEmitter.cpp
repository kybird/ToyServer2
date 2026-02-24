#include "Game/Emitter/AuraEmitter.h"

namespace SimpleGame {

AuraEmitter::AuraEmitter(float initialTimer) : _timer(initialTimer)
{
}

void AuraEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
