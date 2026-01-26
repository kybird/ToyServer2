#pragma once
namespace SimpleGame {

struct LoginRequestEvent
{
    uint64_t sessionId;
    std::string username;
    std::string password;
};

} // namespace SimpleGame
