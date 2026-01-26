#pragma once

#include "System/PacketView.h"
#include "System/Session/SessionContext.h"

namespace SimpleGame {
namespace Handlers {
namespace Lobby {

class EnterLobbyHandler
{
public:
    static void Handle(System::SessionContext &ctx, System::PacketView packet);
};

} // namespace Lobby
} // namespace Handlers
} // namespace SimpleGame
