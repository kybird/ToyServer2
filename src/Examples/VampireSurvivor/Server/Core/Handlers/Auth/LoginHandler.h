#pragma once

#include "System/ISession.h"
#include "System/PacketView.h"

namespace SimpleGame {
namespace Handlers {
namespace Auth {

class LoginHandler
{
public:
    static void Handle(System::ISession* session, System::PacketView packet);
};

} // namespace Auth
} // namespace Handlers
} // namespace SimpleGame
