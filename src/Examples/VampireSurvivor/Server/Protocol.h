#pragma once
#include <cstdint>

namespace SimpleGame {

enum PacketID : uint16_t {
    C_LOGIN_REQ = 1,
    S_LOGIN_RES = 2,
    C_MOVE = 3,
    S_MOVE = 4,
    S_ENTER_ROOM = 5,
};

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t size; // Total packet size including header
    uint16_t id;   // Packet ID
};
#pragma pack(pop)

} // namespace SimpleGame
