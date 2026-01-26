#pragma once

#include "System/PacketView.h"
#include "System/Session/SessionContext.h"

namespace SimpleGame {
namespace Handlers {
namespace Game {

class ChatHandler
{
public:
    static void Handle(System::SessionContext &ctx, System::PacketView packet);
};

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
