#include "Game/Emitter/AoEEmitter.h"

#include "Entity/Player.h"

namespace SimpleGame {

AoEEmitter::AoEEmitter(float initialTimer) : _timer(initialTimer)
{
}

void AoEEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, ::System::RefPtr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
