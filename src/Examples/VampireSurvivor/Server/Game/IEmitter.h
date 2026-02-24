#pragma once

#include "Math/Vector2.h"
#include <memory>
#include <string>

namespace SimpleGame {

class Player;
class Room;
class DamageEmitter;

struct WeaponStats
{
    int32_t damage = 0;
    float hitRadius = 1.0f;
    float tickInterval = 1.0f;
    float activeDuration = 0.0f;
    float dotInterval = 0.5f;

    int32_t projectileCount = 1;
    int32_t pierceCount = 0;
    float speedMult = 1.0f;
    float areaMult = 1.0f;
    float durationMult = 1.0f;
    float critChance = 0.0f;
    float critDamageMult = 2.0f;
    int32_t maxTargets = 1;

    std::string effectType;
    float effectValue = 0.0f;
    float effectDuration = 0.0f;

    float arcDegrees = 30.0f;
    float width = 1.0f;
    float height = 1.0f;
    bool bidirectional = false;
    std::string targetRule = "Nearest";
};

class IEmitter
{
public:
    virtual ~IEmitter() = default;

    virtual void
    Update(float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats) = 0;
};

} // namespace SimpleGame
