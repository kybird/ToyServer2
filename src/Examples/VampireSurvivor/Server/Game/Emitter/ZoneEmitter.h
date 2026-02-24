#pragma once
#include "Game/IEmitter.h"

namespace SimpleGame {

class ZoneEmitter : public IEmitter
{
    float _timer = 0.0f;

public:
    ZoneEmitter(float initialTimer = 0.0f);
    void Update(
        float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
    ) override;
};

} // namespace SimpleGame
