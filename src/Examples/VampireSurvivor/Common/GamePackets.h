#pragma once

#include "Protocol.h"
#include "Protocol/game.pb.h"
#include "System/Packet.h"

namespace SimpleGame {

// --- S_LOGIN ---
class S_LoginPacket : public System::ProtobufPacketBase<S_LoginPacket, PacketHeader, Protocol::S_Login>
{
    using Base = System::ProtobufPacketBase<S_LoginPacket, PacketHeader, Protocol::S_Login>;

public:
    static constexpr uint16_t ID = PacketID::S_LOGIN;
    using Base::Base;
};

// --- S_CREATE_ROOM ---
class S_CreateRoomPacket : public System::ProtobufPacketBase<S_CreateRoomPacket, PacketHeader, Protocol::S_CreateRoom>
{
    using Base = System::ProtobufPacketBase<S_CreateRoomPacket, PacketHeader, Protocol::S_CreateRoom>;

public:
    static constexpr uint16_t ID = PacketID::S_CREATE_ROOM;
    using Base::Base;
};

// --- S_JOIN_ROOM ---
class S_JoinRoomPacket : public System::ProtobufPacketBase<S_JoinRoomPacket, PacketHeader, Protocol::S_JoinRoom>
{
    using Base = System::ProtobufPacketBase<S_JoinRoomPacket, PacketHeader, Protocol::S_JoinRoom>;

public:
    static constexpr uint16_t ID = PacketID::S_JOIN_ROOM;
    using Base::Base;
};

// --- S_ROOM_LIST ---
class S_RoomListPacket : public System::ProtobufPacketBase<S_RoomListPacket, PacketHeader, Protocol::S_RoomList>
{
    using Base = System::ProtobufPacketBase<S_RoomListPacket, PacketHeader, Protocol::S_RoomList>;

public:
    static constexpr uint16_t ID = PacketID::S_ROOM_LIST;
    using Base::Base;
};

// --- S_ENTER_LOBBY ---
class S_EnterLobbyPacket : public System::ProtobufPacketBase<S_EnterLobbyPacket, PacketHeader, Protocol::S_EnterLobby>
{
    using Base = System::ProtobufPacketBase<S_EnterLobbyPacket, PacketHeader, Protocol::S_EnterLobby>;

public:
    static constexpr uint16_t ID = PacketID::S_ENTER_LOBBY;
    using Base::Base;
};

// --- S_LEAVE_ROOM ---
class S_LeaveRoomPacket : public System::ProtobufPacketBase<S_LeaveRoomPacket, PacketHeader, Protocol::S_LeaveRoom>
{
    using Base = System::ProtobufPacketBase<S_LeaveRoomPacket, PacketHeader, Protocol::S_LeaveRoom>;

public:
    static constexpr uint16_t ID = PacketID::S_LEAVE_ROOM;
    using Base::Base;
};

// --- S_CHAT ---
class S_ChatPacket : public System::ProtobufPacketBase<S_ChatPacket, PacketHeader, Protocol::S_Chat>
{
    using Base = System::ProtobufPacketBase<S_ChatPacket, PacketHeader, Protocol::S_Chat>;

public:
    static constexpr uint16_t ID = PacketID::S_CHAT;
    using Base::Base;
};

// --- S_SPAWN_OBJECT ---
class S_SpawnObjectPacket
    : public System::ProtobufPacketBase<S_SpawnObjectPacket, PacketHeader, Protocol::S_SpawnObject>
{
    using Base = System::ProtobufPacketBase<S_SpawnObjectPacket, PacketHeader, Protocol::S_SpawnObject>;

public:
    static constexpr uint16_t ID = PacketID::S_SPAWN_OBJECT;
    using Base::Base;
};

// --- S_DESPAWN_OBJECT ---
class S_DespawnObjectPacket
    : public System::ProtobufPacketBase<S_DespawnObjectPacket, PacketHeader, Protocol::S_DespawnObject>
{
    using Base = System::ProtobufPacketBase<S_DespawnObjectPacket, PacketHeader, Protocol::S_DespawnObject>;

public:
    static constexpr uint16_t ID = PacketID::S_DESPAWN_OBJECT;
    using Base::Base;
};

// --- S_MOVE_OBJECT_BATCH ---
class S_MoveObjectBatchPacket
    : public System::ProtobufPacketBase<S_MoveObjectBatchPacket, PacketHeader, Protocol::S_MoveObjectBatch>
{
    using Base = System::ProtobufPacketBase<S_MoveObjectBatchPacket, PacketHeader, Protocol::S_MoveObjectBatch>;

public:
    static constexpr uint16_t ID = PacketID::S_MOVE_OBJECT_BATCH;
    using Base::Base;
};

// --- S_DAMAGE_EFFECT ---
class S_DamageEffectPacket
    : public System::ProtobufPacketBase<S_DamageEffectPacket, PacketHeader, Protocol::S_DamageEffect>
{
    using Base = System::ProtobufPacketBase<S_DamageEffectPacket, PacketHeader, Protocol::S_DamageEffect>;

public:
    static constexpr uint16_t ID = PacketID::S_DAMAGE_EFFECT;
    using Base::Base;
};

// --- S_KNOCKBACK ---
class S_KnockbackPacket : public System::ProtobufPacketBase<S_KnockbackPacket, PacketHeader, Protocol::S_Knockback>
{
    using Base = System::ProtobufPacketBase<S_KnockbackPacket, PacketHeader, Protocol::S_Knockback>;

public:
    static constexpr uint16_t ID = PacketID::S_KNOCKBACK;
    using Base::Base;
};

// --- S_PING ---
class S_PingPacket : public System::ProtobufPacketBase<S_PingPacket, PacketHeader, Protocol::S_Ping>
{
    using Base = System::ProtobufPacketBase<S_PingPacket, PacketHeader, Protocol::S_Ping>;

public:
    static constexpr uint16_t ID = PacketID::S_PING;
    using Base::Base;
};

// --- S_PLAYER_STATE_ACK ---
class S_PlayerStateAckPacket
    : public System::ProtobufPacketBase<S_PlayerStateAckPacket, PacketHeader, Protocol::S_PlayerStateAck>
{
    using Base = System::ProtobufPacketBase<S_PlayerStateAckPacket, PacketHeader, Protocol::S_PlayerStateAck>;

public:
    static constexpr uint16_t ID = PacketID::S_PLAYER_STATE_ACK;
    using Base::Base;
};

// --- S_GAME_OVER ---
class S_GameOverPacket : public System::ProtobufPacketBase<S_GameOverPacket, PacketHeader, Protocol::S_GameOver>
{
    using Base = System::ProtobufPacketBase<S_GameOverPacket, PacketHeader, Protocol::S_GameOver>;

public:
    static constexpr uint16_t ID = PacketID::S_GAME_OVER;
    using Base::Base;
};

// --- S_GAME_WIN ---
class S_GameWinPacket : public System::ProtobufPacketBase<S_GameWinPacket, PacketHeader, Protocol::S_GameWin>
{
    using Base = System::ProtobufPacketBase<S_GameWinPacket, PacketHeader, Protocol::S_GameWin>;

public:
    static constexpr uint16_t ID = PacketID::S_GAME_WIN;
    using Base::Base;
};

// --- S_PLAYER_DEAD ---
class S_PlayerDeadPacket : public System::ProtobufPacketBase<S_PlayerDeadPacket, PacketHeader, Protocol::S_PlayerDead>
{
    using Base = System::ProtobufPacketBase<S_PlayerDeadPacket, PacketHeader, Protocol::S_PlayerDead>;

public:
    static constexpr uint16_t ID = PacketID::S_PLAYER_DEAD;
    using Base::Base;
};

// --- S_EXP_CHANGE ---
class S_ExpChangePacket : public System::ProtobufPacketBase<S_ExpChangePacket, PacketHeader, Protocol::S_ExpChange>
{
    using Base = System::ProtobufPacketBase<S_ExpChangePacket, PacketHeader, Protocol::S_ExpChange>;

public:
    static constexpr uint16_t ID = PacketID::S_EXP_CHANGE;
    using Base::Base;
};

// --- S_HP_CHANGE ---
class S_HpChangePacket : public System::ProtobufPacketBase<S_HpChangePacket, PacketHeader, Protocol::S_HpChange>
{
    using Base = System::ProtobufPacketBase<S_HpChangePacket, PacketHeader, Protocol::S_HpChange>;

public:
    static constexpr uint16_t ID = PacketID::S_HP_CHANGE;
    using Base::Base;
};

// --- S_WAVE_NOTIFY ---
class S_WaveNotifyPacket : public System::ProtobufPacketBase<S_WaveNotifyPacket, PacketHeader, Protocol::S_WaveNotify>
{
    using Base = System::ProtobufPacketBase<S_WaveNotifyPacket, PacketHeader, Protocol::S_WaveNotify>;

public:
    static constexpr uint16_t ID = PacketID::S_WAVE_NOTIFY;
    using Base::Base;
};

// --- S_LEVEL_UP_OPTION ---
class S_LevelUpOptionPacket
    : public System::ProtobufPacketBase<S_LevelUpOptionPacket, PacketHeader, Protocol::S_LevelUpOption>
{
    using Base = System::ProtobufPacketBase<S_LevelUpOptionPacket, PacketHeader, Protocol::S_LevelUpOption>;

public:
    static constexpr uint16_t ID = PacketID::S_LEVEL_UP_OPTION;
    using Base::Base;
};

// --- C_SELECT_LEVEL_UP ---
class C_SelectLevelUpPacket
    : public System::ProtobufPacketBase<C_SelectLevelUpPacket, PacketHeader, Protocol::C_SelectLevelUp>
{
    using Base = System::ProtobufPacketBase<C_SelectLevelUpPacket, PacketHeader, Protocol::C_SelectLevelUp>;

public:
    static constexpr uint16_t ID = PacketID::C_SELECT_LEVEL_UP;
    using Base::Base;
};

// --- CLIENT PACKETS (For Client Use) ---

// --- C_LOGIN ---
class C_LoginPacket : public System::ProtobufPacketBase<C_LoginPacket, PacketHeader, Protocol::C_Login>
{
    using Base = System::ProtobufPacketBase<C_LoginPacket, PacketHeader, Protocol::C_Login>;

public:
    static constexpr uint16_t ID = PacketID::C_LOGIN;
    using Base::Base;
};

// --- C_ENTER_LOBBY ---
class C_EnterLobbyPacket : public System::ProtobufPacketBase<C_EnterLobbyPacket, PacketHeader, Protocol::C_EnterLobby>
{
    using Base = System::ProtobufPacketBase<C_EnterLobbyPacket, PacketHeader, Protocol::C_EnterLobby>;

public:
    static constexpr uint16_t ID = PacketID::C_ENTER_LOBBY;
    using Base::Base;
};

// --- C_CREATE_ROOM ---
class C_CreateRoomPacket : public System::ProtobufPacketBase<C_CreateRoomPacket, PacketHeader, Protocol::C_CreateRoom>
{
    using Base = System::ProtobufPacketBase<C_CreateRoomPacket, PacketHeader, Protocol::C_CreateRoom>;

public:
    static constexpr uint16_t ID = PacketID::C_CREATE_ROOM;
    using Base::Base;
};

// --- C_GET_ROOM_LIST ---
class C_GetRoomListPacket
    : public System::ProtobufPacketBase<C_GetRoomListPacket, PacketHeader, Protocol::C_GetRoomList>
{
    using Base = System::ProtobufPacketBase<C_GetRoomListPacket, PacketHeader, Protocol::C_GetRoomList>;

public:
    static constexpr uint16_t ID = PacketID::C_GET_ROOM_LIST;
    using Base::Base;
};

// --- C_LEAVE_ROOM ---
class C_LeaveRoomPacket : public System::ProtobufPacketBase<C_LeaveRoomPacket, PacketHeader, Protocol::C_LeaveRoom>
{
    using Base = System::ProtobufPacketBase<C_LeaveRoomPacket, PacketHeader, Protocol::C_LeaveRoom>;

public:
    static constexpr uint16_t ID = PacketID::C_LEAVE_ROOM;
    using Base::Base;
};

// --- C_CHAT ---
class C_ChatPacket : public System::ProtobufPacketBase<C_ChatPacket, PacketHeader, Protocol::C_Chat>
{
    using Base = System::ProtobufPacketBase<C_ChatPacket, PacketHeader, Protocol::C_Chat>;

public:
    static constexpr uint16_t ID = PacketID::C_CHAT;
    using Base::Base;
};

// --- C_PONG ---
class C_PongPacket : public System::ProtobufPacketBase<C_PongPacket, PacketHeader, Protocol::C_Pong>
{
    using Base = System::ProtobufPacketBase<C_PongPacket, PacketHeader, Protocol::C_Pong>;

public:
    static constexpr uint16_t ID = PacketID::C_PONG;
    using Base::Base;
};

// --- C_PING ---
class C_PingPacket : public System::ProtobufPacketBase<C_PingPacket, PacketHeader, Protocol::C_Ping>
{
    using Base = System::ProtobufPacketBase<C_PingPacket, PacketHeader, Protocol::C_Ping>;

public:
    static constexpr uint16_t ID = PacketID::C_PING;
    using Base::Base;
};

// --- S_PONG ---
class S_PongPacket : public System::ProtobufPacketBase<S_PongPacket, PacketHeader, Protocol::S_Pong>
{
    using Base = System::ProtobufPacketBase<S_PongPacket, PacketHeader, Protocol::S_Pong>;

public:
    static constexpr uint16_t ID = PacketID::S_PONG;
    using Base::Base;
};

// --- C_GAME_READY ---
class C_GameReadyPacket : public System::ProtobufPacketBase<C_GameReadyPacket, PacketHeader, Protocol::C_GameReady>
{
    using Base = System::ProtobufPacketBase<C_GameReadyPacket, PacketHeader, Protocol::C_GameReady>;

public:
    static constexpr uint16_t ID = PacketID::C_GAME_READY;
    using Base::Base;
};

// --- S_DEBUG_SERVER_TICK ---
class S_DebugServerTickPacket
    : public System::ProtobufPacketBase<S_DebugServerTickPacket, PacketHeader, Protocol::S_DebugServerTick>
{
    using Base = System::ProtobufPacketBase<S_DebugServerTickPacket, PacketHeader, Protocol::S_DebugServerTick>;

public:
    static constexpr uint16_t ID = PacketID::S_DEBUG_SERVER_TICK;
    using Base::Base;
};

} // namespace SimpleGame
