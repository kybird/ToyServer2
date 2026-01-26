#pragma once

#include "System/PacketView.h"
#include "System/Session/SessionContext.h"

namespace SimpleGame {
namespace Handlers {
namespace Room {

class CreateRoomHandler
{
public:
    static void Handle(System::SessionContext &ctx, System::PacketView packet);
};

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
