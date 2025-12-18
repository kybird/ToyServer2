#pragma once
#include <cstdint>

namespace SimpleGame {

enum PacketID : uint16_t {
    C_LOGIN = 100,
    S_LOGIN = 101,
    C_CREATE_ROOM = 102,
    S_CREATE_ROOM = 103,
    C_JOIN_ROOM = 104,
    S_JOIN_ROOM = 105,

    C_ENTER_LOBBY = 110,
    S_ENTER_LOBBY = 111,
    C_LEAVE_ROOM = 112,
    S_LEAVE_ROOM = 113,

    C_CHAT = 120,
    S_CHAT = 121,

    S_SPAWN_OBJECT = 200,
    S_DESPAWN_OBJECT = 201,
    S_MOVE_OBJECT_BATCH = 202,
    C_MOVE = 203,

    C_USE_SKILL = 300,
    S_SKILL_EFFECT = 301,
    S_DAMAGE_EFFECT = 302,
    S_PLAYER_DOWNED = 303,
    S_PLAYER_REVIVE = 304,

    S_EXP_CHANGE = 400,
    S_LEVEL_UP_OPTION = 401,
    C_SELECT_LEVEL_UP = 402,

    S_GAME_WIN = 500,
    S_GAME_OVER = 501,
};

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t size; // Total packet size including header
    uint16_t id;   // Packet ID
};
#pragma pack(pop)

} // namespace SimpleGame
