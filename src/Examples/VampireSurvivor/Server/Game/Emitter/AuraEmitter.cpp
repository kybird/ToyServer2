#include "Game/Emitter/AuraEmitter.h"

#include "Entity/Player.h"

namespace SimpleGame {

AuraEmitter::AuraEmitter(float initialTimer) : _timer(initialTimer)
{
}

void AuraEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, ::System::RefPtr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
