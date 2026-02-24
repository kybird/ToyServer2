#include "Game/Emitter/EmitterFactory.h"
#include "Game/Emitter/AoEEmitter.h"
#include "Game/Emitter/ArcEmitter.h"
#include "Game/Emitter/AuraEmitter.h"
#include "Game/Emitter/DirectionalEmitter.h"
#include "Game/Emitter/FieldStateEmitter.h"
#include "Game/Emitter/LinearEmitter.h"
#include "Game/Emitter/OrbitEmitter.h"
#include "Game/Emitter/ZoneEmitter.h"


namespace SimpleGame {

std::unique_ptr<IEmitter>
EmitterFactory::CreateEmitter(const std::string &type, float initialTickInterval, float activeDuration)
{
    if (type == "Linear")
        return std::make_unique<LinearEmitter>(initialTickInterval);
    if (type == "Orbit")
        return std::make_unique<OrbitEmitter>(initialTickInterval);
    if (type == "Zone")
        return std::make_unique<ZoneEmitter>(initialTickInterval);
    if (type == "DirectionalMelee")
        return std::make_unique<DirectionalEmitter>(initialTickInterval);
    if (type == "ArcMelee")
        return std::make_unique<ArcEmitter>(initialTickInterval);
    if (type == "Aura")
        return std::make_unique<AuraEmitter>(initialTickInterval);
    if (type == "AoE")
        return std::make_unique<AoEEmitter>(initialTickInterval);
    if (type == "FieldState")
        return std::make_unique<FieldStateEmitter>(initialTickInterval);

    return std::make_unique<LinearEmitter>(initialTickInterval);
}

} // namespace SimpleGame
