#pragma once

#include "System/ISession.h"
#include "System/PacketView.h"

namespace SimpleGame {
namespace Handlers {
namespace Room {

class JoinRoomHandler
{
public:
    static void Handle(System::ISession* session, System::PacketView packet);
};

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
