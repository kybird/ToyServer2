#pragma once

#include "Protocol.h"
#include "Protocol/game.pb.h"
#include "System/Packet.h"

namespace SimpleGame {

// Generic Template for Protobuf Packets
template <uint16_t PacketId, typename TProto>
class ProtobufPacket : public System::ProtobufPacketBase<ProtobufPacket<PacketId, TProto>, PacketHeader, TProto>
{
public:
    static constexpr uint16_t ID = PacketId;
    using System::ProtobufPacketBase<ProtobufPacket<PacketId, TProto>, PacketHeader, TProto>::ProtobufPacketBase;
};

using C_LoginPacket = ProtobufPacket<PacketID::C_LOGIN, Protocol::C_Login>;
using S_LoginPacket = ProtobufPacket<PacketID::S_LOGIN, Protocol::S_Login>;
using C_CreateRoomPacket = ProtobufPacket<PacketID::C_CREATE_ROOM, Protocol::C_CreateRoom>;
using S_CreateRoomPacket = ProtobufPacket<PacketID::S_CREATE_ROOM, Protocol::S_CreateRoom>;
using C_JoinRoomPacket = ProtobufPacket<PacketID::C_JOIN_ROOM, Protocol::C_JoinRoom>;
using S_JoinRoomPacket = ProtobufPacket<PacketID::S_JOIN_ROOM, Protocol::S_JoinRoom>;
using C_GetRoomListPacket = ProtobufPacket<PacketID::C_GET_ROOM_LIST, Protocol::C_GetRoomList>;
using S_RoomListPacket = ProtobufPacket<PacketID::S_ROOM_LIST, Protocol::S_RoomList>;
using C_EnterLobbyPacket = ProtobufPacket<PacketID::C_ENTER_LOBBY, Protocol::C_EnterLobby>;
using S_EnterLobbyPacket = ProtobufPacket<PacketID::S_ENTER_LOBBY, Protocol::S_EnterLobby>;
using C_LeaveRoomPacket = ProtobufPacket<PacketID::C_LEAVE_ROOM, Protocol::C_LeaveRoom>;
using S_LeaveRoomPacket = ProtobufPacket<PacketID::S_LEAVE_ROOM, Protocol::S_LeaveRoom>;
using C_GameReadyPacket = ProtobufPacket<PacketID::C_GAME_READY, Protocol::C_GameReady>;
using C_ChatPacket = ProtobufPacket<PacketID::C_CHAT, Protocol::C_Chat>;
using S_ChatPacket = ProtobufPacket<PacketID::S_CHAT, Protocol::S_Chat>;
using S_SpawnObjectPacket = ProtobufPacket<PacketID::S_SPAWN_OBJECT, Protocol::S_SpawnObject>;
using S_DespawnObjectPacket = ProtobufPacket<PacketID::S_DESPAWN_OBJECT, Protocol::S_DespawnObject>;
using S_MoveObjectBatchPacket = ProtobufPacket<PacketID::S_MOVE_OBJECT_BATCH, Protocol::S_MoveObjectBatch>;
using C_MoveInputPacket = ProtobufPacket<PacketID::C_MOVE_INPUT, Protocol::C_MoveInput>;
using S_PlayerStateAckPacket = ProtobufPacket<PacketID::S_PLAYER_STATE_ACK, Protocol::S_PlayerStateAck>;
using C_UseSkillPacket = ProtobufPacket<PacketID::C_USE_SKILL, Protocol::C_UseSkill>;
using S_SkillEffectPacket = ProtobufPacket<PacketID::S_SKILL_EFFECT, Protocol::S_SkillEffect>;
using S_DamageEffectPacket = ProtobufPacket<PacketID::S_DAMAGE_EFFECT, Protocol::S_DamageEffect>;
using S_KnockbackPacket = ProtobufPacket<PacketID::S_KNOCKBACK, Protocol::S_Knockback>;
using S_PlayerDownedPacket = ProtobufPacket<PacketID::S_PLAYER_DOWNED, Protocol::S_PlayerDowned>;
using S_PlayerRevivePacket = ProtobufPacket<PacketID::S_PLAYER_REVIVE, Protocol::S_PlayerRevive>;
using S_ExpChangePacket = ProtobufPacket<PacketID::S_EXP_CHANGE, Protocol::S_ExpChange>;
using S_LevelUpOptionPacket = ProtobufPacket<PacketID::S_LEVEL_UP_OPTION, Protocol::S_LevelUpOption>;
using C_SelectLevelUpPacket = ProtobufPacket<PacketID::C_SELECT_LEVEL_UP, Protocol::C_SelectLevelUp>;
using S_HpChangePacket = ProtobufPacket<PacketID::S_HP_CHANGE, Protocol::S_HpChange>;
using S_WaveNotifyPacket = ProtobufPacket<PacketID::S_WAVE_NOTIFY, Protocol::S_WaveNotify>;
using S_GameWinPacket = ProtobufPacket<PacketID::S_GAME_WIN, Protocol::S_GameWin>;
using S_GameOverPacket = ProtobufPacket<PacketID::S_GAME_OVER, Protocol::S_GameOver>;
using S_PlayerDeadPacket = ProtobufPacket<PacketID::S_PLAYER_DEAD, Protocol::S_PlayerDead>;
using S_PingPacket = ProtobufPacket<PacketID::S_PING, Protocol::S_Ping>;
using C_PongPacket = ProtobufPacket<PacketID::C_PONG, Protocol::C_Pong>;
using C_PingPacket = ProtobufPacket<PacketID::C_PING, Protocol::C_Ping>;
using S_PongPacket = ProtobufPacket<PacketID::S_PONG, Protocol::S_Pong>;
using S_DebugServerTickPacket = ProtobufPacket<PacketID::S_DEBUG_SERVER_TICK, Protocol::S_DebugServerTick>;
using S_UpdateInventoryPacket = ProtobufPacket<PacketID::S_UPDATE_INVENTORY, Protocol::S_UpdateInventory>;

} // namespace SimpleGame
