#pragma once

#include "System/PacketView.h"
#include "System/Session/SessionContext.h"

namespace SimpleGame {
namespace Handlers {
namespace Auth {

class LoginHandler
{
public:
    static void Handle(System::SessionContext &ctx, System::PacketView packet);
};

} // namespace Auth
} // namespace Handlers
} // namespace SimpleGame
