#include "Game/Emitter/DirectionalEmitter.h"

#include "Entity/Player.h"

namespace SimpleGame {

DirectionalEmitter::DirectionalEmitter(float initialTimer) : _timer(initialTimer)
{
}

void DirectionalEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, ::System::RefPtr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
