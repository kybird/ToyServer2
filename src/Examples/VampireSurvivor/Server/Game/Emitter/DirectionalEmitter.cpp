#include "Game/Emitter/DirectionalEmitter.h"

namespace SimpleGame {

DirectionalEmitter::DirectionalEmitter(float initialTimer) : _timer(initialTimer)
{
}

void DirectionalEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
