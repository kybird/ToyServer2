#pragma once

#include "System/ISession.h"
#include "System/PacketView.h"

namespace SimpleGame {
namespace Handlers {
namespace System {

class PingHandler
{
public:
    static void Handle(::System::ISession* session, ::System::PacketView packet);
};

} // namespace System
} // namespace Handlers
} // namespace SimpleGame
