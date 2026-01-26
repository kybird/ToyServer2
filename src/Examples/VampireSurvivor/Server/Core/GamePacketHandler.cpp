#include "Core/GamePacketHandler.h"
#include "Entity/Player.h"
#include "Game/Room.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "System/ILog.h"

// Handlers
#include "Handlers/Auth/LoginHandler.h"
#include "Handlers/Game/ChatHandler.h"
#include "Handlers/Game/GameReadyHandler.h"
#include "Handlers/Game/MoveInputHandler.h"
#include "Handlers/Game/SelectLevelUpHandler.h"
#include "Handlers/Lobby/EnterLobbyHandler.h"
#include "Handlers/Room/CreateRoomHandler.h"
#include "Handlers/Room/GetRoomListHandler.h"
#include "Handlers/Room/JoinRoomHandler.h"
#include "Handlers/Room/LeaveRoomHandler.h"
#include "Handlers/System/PingHandler.h"
#include "Handlers/System/PongHandler.h"

namespace SimpleGame {

GamePacketHandler::GamePacketHandler()
{
    // Explicitly qualify references to avoid ambiguity with SimpleGame::Room class
    _handlers[PacketID::C_LOGIN] = Handlers::Auth::LoginHandler::Handle;
    _handlers[PacketID::C_ENTER_LOBBY] = Handlers::Lobby::EnterLobbyHandler::Handle;

    _handlers[PacketID::C_CREATE_ROOM] = Handlers::Room::CreateRoomHandler::Handle;
    _handlers[PacketID::C_GET_ROOM_LIST] = Handlers::Room::GetRoomListHandler::Handle;
    _handlers[PacketID::C_JOIN_ROOM] = Handlers::Room::JoinRoomHandler::Handle;
    _handlers[PacketID::C_LEAVE_ROOM] = Handlers::Room::LeaveRoomHandler::Handle;

    _handlers[PacketID::C_MOVE_INPUT] = Handlers::Game::MoveInputHandler::Handle;
    _handlers[PacketID::C_GAME_READY] = Handlers::Game::GameReadyHandler::Handle;
    _handlers[PacketID::C_CHAT] = Handlers::Game::ChatHandler::Handle;
    _handlers[PacketID::C_SELECT_LEVEL_UP] = Handlers::Game::SelectLevelUpHandler::Handle;

    _handlers[PacketID::C_PING] = Handlers::System::PingHandler::Handle;
    _handlers[PacketID::C_PONG] = Handlers::System::PongHandler::Handle;
}

void GamePacketHandler::HandlePacket(System::SessionContext ctx, System::PacketView packet)
{
    auto it = _handlers.find(packet.GetId());
    if (it != _handlers.end())
    {
        it->second(ctx, packet);
    }
    else
    {
        LOG_ERROR("Unknown Packet ID: {}", packet.GetId());
    }
}

void GamePacketHandler::OnSessionDisconnect(System::SessionContext ctx)
{
    uint64_t sessionId = ctx.Id();
    LOG_INFO("Session {} Disconnected. Cleaning up...", sessionId);

    // 1. Remove from Lobby
    if (RoomManager::Instance().IsInLobby(sessionId))
    {
        RoomManager::Instance().LeaveLobby(sessionId);
        LOG_INFO("Session {} removed from Lobby.", sessionId);
    }

    // 2. Remove from Room/Game
    auto player = RoomManager::Instance().GetPlayer(sessionId);
    if (player)
    {
        // Safe to call Room::Leave as it is thread-safe (internal mutex)
        int roomId = player->GetRoomId();
        auto room = RoomManager::Instance().GetRoom(roomId);
        if (room)
        {
            room->Leave(sessionId);
        }

        // Unregister from global map
        RoomManager::Instance().UnregisterPlayer(sessionId);
        LOG_INFO("Session {} unregistered from Player Map.", sessionId);
    }
}

} // namespace SimpleGame
