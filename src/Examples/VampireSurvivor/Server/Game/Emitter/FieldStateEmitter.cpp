#include "Game/Emitter/FieldStateEmitter.h"

namespace SimpleGame {

FieldStateEmitter::FieldStateEmitter(float initialTimer) : _timer(initialTimer)
{
}

void FieldStateEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
)
{
    // Empty for now to just compile correctly
}

} // namespace SimpleGame
