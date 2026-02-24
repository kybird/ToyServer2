#pragma once
#include "Game/IEmitter.h"
#include <memory>
#include <string>

namespace SimpleGame {

class EmitterFactory
{
public:
    static std::unique_ptr<IEmitter>
    CreateEmitter(const std::string &type, float initialTickInterval, float activeDuration);
};

} // namespace SimpleGame
