#pragma once
#include "Game/IEmitter.h"

namespace SimpleGame {

class FieldStateEmitter : public IEmitter
{
    enum class EmitterState { COOLING, ACTIVE };
    EmitterState _state = EmitterState::COOLING;
    float _timer = 0.0f;
    float _dotTimer = 0.0f;

public:
    FieldStateEmitter(float initialTimer = 0.0f);
    void Update(
        float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
    ) override;
};

} // namespace SimpleGame
