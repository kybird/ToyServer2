#pragma once
#include <string>

namespace System {
class ISession;
}

namespace SimpleGame {

struct LoginRequestEvent
{
    uint64_t sessionId;
    System::ISession *session;
    std::string username;
    std::string password;
};

} // namespace SimpleGame
