#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace SimpleGame {

class Player; // Forward declaration

struct LoginRequestEvent
{
    uint64_t sessionId;
    std::string username;
    std::string password;
};

// --- System Events for EventBus ---

struct SessionDisconnectedEvent
{
    uint64_t sessionId;
};

struct RoomJoinedEvent
{
    uint64_t sessionId;
    int roomId;
    std::shared_ptr<Player> player;
};

struct RoomLeftEvent
{
    uint64_t sessionId;
};

} // namespace SimpleGame
