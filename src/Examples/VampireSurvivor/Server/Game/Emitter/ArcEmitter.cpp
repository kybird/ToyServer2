#include "Game/Emitter/ArcEmitter.h"

#include "Entity/Player.h"

namespace SimpleGame {

ArcEmitter::ArcEmitter(float initialTimer) : _timer(initialTimer)
{
}

void ArcEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, ::System::RefPtr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
