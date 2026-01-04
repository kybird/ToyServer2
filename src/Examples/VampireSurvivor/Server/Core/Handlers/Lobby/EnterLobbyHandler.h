#pragma once

#include "System/ISession.h"
#include "System/PacketView.h"

namespace SimpleGame {
namespace Handlers {
namespace Lobby {

class EnterLobbyHandler
{
public:
    static void Handle(System::ISession* session, System::PacketView packet);
};

} // namespace Lobby
} // namespace Handlers
} // namespace SimpleGame
