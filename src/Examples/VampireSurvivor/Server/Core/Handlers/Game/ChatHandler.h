#pragma once

#include "System/ISession.h"
#include "System/PacketView.h"

namespace SimpleGame {
namespace Handlers {
namespace Game {

class ChatHandler
{
public:
    static void Handle(System::ISession* session, System::PacketView packet);
};

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
