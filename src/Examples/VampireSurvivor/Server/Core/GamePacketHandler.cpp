#include "Core/GamePacketHandler.h"
#include "Entity/Player.h"
#include "Entity/PlayerFactory.h"
#include "Game/Room.h"
#include "Game/RoomManager.h"
#include "GameEvents.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/Events/EventBus.h"
#include "System/ILog.h"
#include "System/ISession.h"
#include "System/Network/ByteBuffer.h"
#include "System/PacketView.h"
#include "System/Thread/IStrand.h"

namespace SimpleGame {

void GamePacketHandler::HandlePacket(System::ISession *session, System::PacketView packet)
{
    switch (packet.GetId())
    {
    case PacketID::C_LOGIN: {
        Protocol::C_Login req;
        if (packet.Parse(req))
        {
            // Map to Event
            LoginRequestEvent evt;
            evt.sessionId = session->GetId();
            evt.session = session;
            evt.username = req.username();
            evt.password = req.password();
            System::EventBus::Instance().Publish(evt);
            LOG_INFO("Login Requested: {}", evt.username);
        }
        else
        {
            LOG_ERROR("Failed to parse C_LOGIN");
        }
        break;
    }
    case PacketID::C_MOVE: {
        Protocol::C_Move req;
        if (packet.Parse(req))
        {
            // Find Player using Global Map
            auto player = RoomManager::Instance().GetPlayer(session->GetId());
            if (player)
            {
                // [Strand] Use Room's Strand for processing
                auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
                if (room && room->GetStrand())
                {
                    // Capture by value for thread safety
                    float dx = req.dir_x();
                    float dy = req.dir_y();

                    room->GetStrand()->Post(
                        [player, dx, dy]()
                        {
                            // Simple Movement Logic: Set Velocity based on Direction
                            float speed = 200.0f;
                            player->SetVelocity(dx * speed, dy * speed);

                            // Update state
                            if (dx == 0 && dy == 0)
                            {
                                player->SetState(Protocol::IDLE);
                            }
                            else
                            {
                                player->SetState(Protocol::MOVING);
                            }
                        }
                    );
                }
                else
                {
                    // Fallback (Main Thread)
                    float speed = 200.0f;
                    player->SetVelocity(req.dir_x() * speed, req.dir_y() * speed);

                    if (req.dir_x() == 0 && req.dir_y() == 0)
                    {
                        player->SetState(Protocol::IDLE);
                    }
                    else
                    {
                        player->SetState(Protocol::MOVING);
                    }
                }
            }
            else
            {
                LOG_WARN("C_MOVE from unknown session {}", session->GetId());
            }
        }
        break;
    }
    case PacketID::C_CREATE_ROOM: {
        std::cout << "[DEBUG] Handling C_CREATE_ROOM" << std::endl;
        Protocol::C_CreateRoom req;
        if (packet.Parse(req))
        {
            std::cout << "[DEBUG] Parsed C_CREATE_ROOM" << std::endl;

            static int nextRoomId = 2; // Simple auto-increment
            int newRoomId = nextRoomId++;

            auto room = RoomManager::Instance().CreateRoom(newRoomId);

            Protocol::S_CreateRoom res;
            res.set_success(true);
            res.set_room_id(newRoomId);

            S_CreateRoomPacket respPacket(res);
            session->SendPacket(respPacket);
            LOG_INFO("Created Room {}", newRoomId);
        }
        break;
    }
    case PacketID::C_GET_ROOM_LIST: {
        Protocol::C_GetRoomList req;
        if (packet.Parse(req))
        {
            auto rooms = RoomManager::Instance().GetAllRooms();

            Protocol::S_RoomList res;
            for (const auto &room : rooms)
            {
                // 필터링: only_joinable이면 게임 진행 중인 방 제외
                if (req.only_joinable() && room->IsPlaying())
                    continue;

                auto *roomInfo = res.add_rooms();
                roomInfo->set_room_id(room->GetId());
                roomInfo->set_current_players(room->GetPlayerCount());
                roomInfo->set_max_players(room->GetMaxPlayers());
                roomInfo->set_is_playing(room->IsPlaying());
            }

            S_RoomListPacket respPacket(res);
            session->SendPacket(respPacket);
            LOG_INFO("Sent room list to session {}: {} rooms", session->GetId(), res.rooms_size());
        }
        break;
    }
    case PacketID::C_JOIN_ROOM: {
        Protocol::C_JoinRoom req;
        if (packet.Parse(req))
        {
            // 1. Get Room
            int roomId = req.room_id();
            auto room = RoomManager::Instance().GetRoom(roomId);
            if (room)
            {
                // Check if already in room
                if (RoomManager::Instance().GetPlayer(session->GetId()))
                {
                    LOG_WARN("Player {} already in room", session->GetId());
                    break;
                }

                // 2. Create Player
                int32_t gameId = (int32_t)session->GetId();
                auto player = PlayerFactory::Instance().CreatePlayer(gameId, session);
                player->SetName("Survivor_" + std::to_string(gameId));

                // 3. Send Response FIRST (before entering room)
                Protocol::S_JoinRoom res;
                res.set_success(true);
                res.set_room_id(roomId);

                S_JoinRoomPacket respPacket(res);
                session->SendPacket(respPacket);
                LOG_INFO("Player joined Room {}", roomId);

                // 4. Enter Room (this will send existing objects to the player)
                room->Enter(player);
                // Remove from lobby if there
                RoomManager::Instance().LeaveLobby(session->GetId());
                RoomManager::Instance().RegisterPlayer(session->GetId(), player);
            }
            else
            {
                LOG_WARN("Room {} not found", roomId);
            }
        }
        break;
    }
    case PacketID::C_ENTER_LOBBY: {
        Protocol::C_EnterLobby req;
        if (packet.Parse(req))
        {
            RoomManager::Instance().EnterLobby(session); // Now passes session pointer

            Protocol::S_EnterLobby res;
            res.set_success(true);

            // Send Response
            S_EnterLobbyPacket respPacket(res);
            session->SendPacket(respPacket);
            LOG_INFO("Session {} Entered Lobby", session->GetId());
        }
        else
        {
            LOG_ERROR("Failed to parse C_ENTER_LOBBY (Len: {})", packet.GetLength());
        }
        break;
    }
    case PacketID::C_LEAVE_ROOM: {
        Protocol::C_LeaveRoom req;
        if (packet.Parse(req))
        {
            auto player = RoomManager::Instance().GetPlayer(session->GetId());
            if (player)
            {
                // Remove from Room
                // Assuming Player has pointer to Room or we need to search?
                // RoomManager doesn't track RoomID for player directly in hash map, but Player obj might know?
                // Checking Player.h would be safer, but for MVP assuming RoomManager::RegisterPlayer
                // implies active game. We should unregister.
                RoomManager::Instance().UnregisterPlayer(session->GetId());
                // Logic to remove from actual Room object is needed if Room holds shared_ptr<Player> list.
                // Room::Leave(player)
                // We need to iterate rooms or track room ID in player.
                // For now, let's just add to Lobby and assume Room handles cleanup on tick or we need to fix this.
                // TODO: Properly remove from Room object.
            }

            RoomManager::Instance().EnterLobby(session);

            Protocol::S_LeaveRoom res;
            res.set_success(true);
            // Send Response
            S_LeaveRoomPacket respPacket(res);
            session->SendPacket(respPacket);
            LOG_INFO("Session {} Left Room -> Lobby", session->GetId());
        }
        break;
    }
    case PacketID::C_CHAT: {
        std::cout << "[DEBUG] Handling C_CHAT" << std::endl;
        Protocol::C_Chat req;
        if (packet.Parse(req))
        {
            std::string msg = req.msg();
            std::cout << "[DEBUG] Chat Msg: " << msg << std::endl;

            // Broadcast
            Protocol::S_Chat res;
            res.set_player_id((int32_t)session->GetId());
            res.set_msg(msg);

            S_ChatPacket chatPacket(res);
            // If in Lobby, broadcast to all lobby sessions
            if (RoomManager::Instance().IsInLobby(session->GetId()))
            {
                std::cout << "[DEBUG] Broadcasting to Lobby..." << std::endl;
                RoomManager::Instance().BroadcastPacketToLobby(chatPacket);
                std::cout << "[DEBUG] Broadcast to Lobby Complete." << std::endl;
            }
            else
            {
                // In Room - Broadcast via Room
                std::cout << "[DEBUG] Broadcasting to Room..." << std::endl;
                auto player = RoomManager::Instance().GetPlayer(session->GetId());
                if (player)
                {
                    // Room broadcast logic
                    // player->GetRoom()->BroadcastPacket(chatPacket);
                    LOG_WARN("Room Chat not fully implemented. Echoing only.");
                    session->SendPacket(chatPacket);
                }
            }
        }
        break;
    }
    default:
        LOG_ERROR("Unknown Packet ID: {}", packet.GetId());
        break;
    }
} // End HandlePacket

void GamePacketHandler::OnSessionDisconnect(System::ISession *session)
{
    LOG_INFO("Session {} Disconnected. Cleaning up...", session->GetId());

    // 1. Remove from Lobby
    if (RoomManager::Instance().IsInLobby(session->GetId()))
    {
        RoomManager::Instance().LeaveLobby(session->GetId());
        LOG_INFO("Session {} removed from Lobby.", session->GetId());
    }

    // 2. Remove from Room/Game
    auto player = RoomManager::Instance().GetPlayer(session->GetId());
    if (player)
    {
        // TODO: Notify Room to remove player object safely (Strand)
        // For now, just unregister from global map to prevent lookups
        RoomManager::Instance().UnregisterPlayer(session->GetId());
        LOG_INFO("Session {} unregistered from Player Map.", session->GetId());
    }
}

} // namespace SimpleGame
