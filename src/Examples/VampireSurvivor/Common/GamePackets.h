#pragma once

#include "Protocol.h"
#include "Protocol/game.pb.h"
#include "System/Packet.h"

namespace SimpleGame {

// --- S_LOGIN ---
class S_LoginPacket : public System::ProtobufPacketBase<S_LoginPacket, PacketHeader, Protocol::S_Login>
{
public:
    static constexpr uint16_t ID = PacketID::S_LOGIN;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_CREATE_ROOM ---
class S_CreateRoomPacket : public System::ProtobufPacketBase<S_CreateRoomPacket, PacketHeader, Protocol::S_CreateRoom>
{
public:
    static constexpr uint16_t ID = PacketID::S_CREATE_ROOM;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_JOIN_ROOM ---
class S_JoinRoomPacket : public System::ProtobufPacketBase<S_JoinRoomPacket, PacketHeader, Protocol::S_JoinRoom>
{
public:
    static constexpr uint16_t ID = PacketID::S_JOIN_ROOM;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_ROOM_LIST ---
class S_RoomListPacket : public System::ProtobufPacketBase<S_RoomListPacket, PacketHeader, Protocol::S_RoomList>
{
public:
    static constexpr uint16_t ID = PacketID::S_ROOM_LIST;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_ENTER_LOBBY ---
class S_EnterLobbyPacket : public System::ProtobufPacketBase<S_EnterLobbyPacket, PacketHeader, Protocol::S_EnterLobby>
{
public:
    static constexpr uint16_t ID = PacketID::S_ENTER_LOBBY;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_LEAVE_ROOM ---
class S_LeaveRoomPacket : public System::ProtobufPacketBase<S_LeaveRoomPacket, PacketHeader, Protocol::S_LeaveRoom>
{
public:
    static constexpr uint16_t ID = PacketID::S_LEAVE_ROOM;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_CHAT ---
class S_ChatPacket : public System::ProtobufPacketBase<S_ChatPacket, PacketHeader, Protocol::S_Chat>
{
public:
    static constexpr uint16_t ID = PacketID::S_CHAT;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_SPAWN_OBJECT ---
class S_SpawnObjectPacket
    : public System::ProtobufPacketBase<S_SpawnObjectPacket, PacketHeader, Protocol::S_SpawnObject>
{
public:
    static constexpr uint16_t ID = PacketID::S_SPAWN_OBJECT;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_DESPAWN_OBJECT ---
class S_DespawnObjectPacket
    : public System::ProtobufPacketBase<S_DespawnObjectPacket, PacketHeader, Protocol::S_DespawnObject>
{
public:
    static constexpr uint16_t ID = PacketID::S_DESPAWN_OBJECT;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_MOVE_OBJECT_BATCH ---
class S_MoveObjectBatchPacket
    : public System::ProtobufPacketBase<S_MoveObjectBatchPacket, PacketHeader, Protocol::S_MoveObjectBatch>
{
public:
    static constexpr uint16_t ID = PacketID::S_MOVE_OBJECT_BATCH;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_DAMAGE_EFFECT ---
class S_DamageEffectPacket
    : public System::ProtobufPacketBase<S_DamageEffectPacket, PacketHeader, Protocol::S_DamageEffect>
{
public:
    static constexpr uint16_t ID = PacketID::S_DAMAGE_EFFECT;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_PING ---
class S_PingPacket : public System::ProtobufPacketBase<S_PingPacket, PacketHeader, Protocol::S_Ping>
{
public:
    static constexpr uint16_t ID = PacketID::S_PING;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- S_PLAYER_STATE_ACK ---
class S_PlayerStateAckPacket
    : public System::ProtobufPacketBase<S_PlayerStateAckPacket, PacketHeader, Protocol::S_PlayerStateAck>
{
public:
    static constexpr uint16_t ID = PacketID::S_PLAYER_STATE_ACK;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- CLIENT PACKETS (For Client Use) ---

// --- C_LOGIN ---
class C_LoginPacket : public System::ProtobufPacketBase<C_LoginPacket, PacketHeader, Protocol::C_Login>
{
public:
    static constexpr uint16_t ID = PacketID::C_LOGIN;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- C_ENTER_LOBBY ---
class C_EnterLobbyPacket : public System::ProtobufPacketBase<C_EnterLobbyPacket, PacketHeader, Protocol::C_EnterLobby>
{
public:
    static constexpr uint16_t ID = PacketID::C_ENTER_LOBBY;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- C_CREATE_ROOM ---
class C_CreateRoomPacket : public System::ProtobufPacketBase<C_CreateRoomPacket, PacketHeader, Protocol::C_CreateRoom>
{
public:
    static constexpr uint16_t ID = PacketID::C_CREATE_ROOM;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- C_GET_ROOM_LIST ---
class C_GetRoomListPacket
    : public System::ProtobufPacketBase<C_GetRoomListPacket, PacketHeader, Protocol::C_GetRoomList>
{
public:
    static constexpr uint16_t ID = PacketID::C_GET_ROOM_LIST;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- C_LEAVE_ROOM ---
class C_LeaveRoomPacket : public System::ProtobufPacketBase<C_LeaveRoomPacket, PacketHeader, Protocol::C_LeaveRoom>
{
public:
    static constexpr uint16_t ID = PacketID::C_LEAVE_ROOM;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- C_CHAT ---
class C_ChatPacket : public System::ProtobufPacketBase<C_ChatPacket, PacketHeader, Protocol::C_Chat>
{
public:
    static constexpr uint16_t ID = PacketID::C_CHAT;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- C_PONG ---
class C_PongPacket : public System::ProtobufPacketBase<C_PongPacket, PacketHeader, Protocol::C_Pong>
{
public:
    static constexpr uint16_t ID = PacketID::C_PONG;
    using ProtobufPacketBase::ProtobufPacketBase;
};

// --- C_GAME_READY ---
class C_GameReadyPacket : public System::ProtobufPacketBase<C_GameReadyPacket, PacketHeader, Protocol::C_GameReady>
{
public:
    static constexpr uint16_t ID = PacketID::C_GAME_READY;
    using ProtobufPacketBase::ProtobufPacketBase;
};

} // namespace SimpleGame
