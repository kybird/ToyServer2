#pragma once

#include "System/PacketView.h"
#include "System/Session/SessionContext.h"

namespace SimpleGame {
namespace Handlers {
namespace System {

class PongHandler
{
public:
    static void Handle(::System::SessionContext &ctx, ::System::PacketView packet);
};

} // namespace System
} // namespace Handlers
} // namespace SimpleGame
