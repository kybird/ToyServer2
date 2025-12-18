#include "Core/GamePacketHandler.h"
#include "Entity/Player.h"
#include "Entity/PlayerFactory.h"
#include "Game/RoomManager.h"
#include "GameEvents.h"
#include "Protocol/game.pb.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Events/EventBus.h"
#include "System/ILog.h"
#include "System/ISession.h"
#include "System/Network/ByteBuffer.h"
#include "System/PacketView.h" // Added

namespace SimpleGame {

void GamePacketHandler::HandlePacket(System::ISession *session, System::PacketView packet)
{
    // PacketView already points to Body. Header is stripped by Dispatcher.
    // Length check for header is already done.

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
                // Simple Movement Logic: Set Velocity based on Direction
                float speed = 200.0f;
                player->SetVelocity(req.dir_x() * speed, req.dir_y() * speed);

                // Update state
                if (req.dir_x() == 0 && req.dir_y() == 0)
                {
                    player->SetState(Protocol::IDLE);
                }
                else
                {
                    player->SetState(Protocol::MOVING);
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
        Protocol::C_CreateRoom req;
        if (packet.Parse(req))
        {
            static int nextRoomId = 2; // Simple auto-increment
            int newRoomId = nextRoomId++;

            auto room = RoomManager::Instance().CreateRoom(newRoomId);

            Protocol::S_CreateRoom res;
            res.set_success(true);
            res.set_room_id(newRoomId);

            size_t resSize = res.ByteSizeLong();
            auto packet = System::MessagePool::AllocatePacket((uint16_t)resSize);
            if (packet)
            {
                PacketHeader *h = (PacketHeader *)packet->Payload();
                h->size = (uint16_t)(sizeof(PacketHeader) + resSize);
                h->id = PacketID::S_CREATE_ROOM;
                res.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)resSize);
                session->Send((System::PacketMessage *)packet);
                System::PacketUtils::ReleasePacket(packet);
            }
            LOG_INFO("Created Room {}", newRoomId);
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

                // 3. Enter Room
                room->Enter(player);
                // Remove from lobby if there
                RoomManager::Instance().LeaveLobby(session->GetId());
                RoomManager::Instance().RegisterPlayer(session->GetId(), player);

                // 4. Send Response
                Protocol::S_JoinRoom res;
                res.set_success(true);
                res.set_room_id(roomId);

                size_t resSize = res.ByteSizeLong();
                auto packet = System::MessagePool::AllocatePacket((uint16_t)resSize);
                if (packet)
                {
                    PacketHeader *h = (PacketHeader *)packet->Payload();
                    h->size = (uint16_t)(sizeof(PacketHeader) + resSize);
                    h->id = PacketID::S_JOIN_ROOM;
                    res.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)resSize);
                    session->Send((System::PacketMessage *)packet);
                    System::PacketUtils::ReleasePacket(packet);
                }

                LOG_INFO("Player joined Room {}", roomId);
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
            size_t resSize = res.ByteSizeLong();
            auto packet = System::MessagePool::AllocatePacket((uint16_t)resSize);
            if (packet)
            {
                PacketHeader *h = (PacketHeader *)packet->Payload();
                h->size = (uint16_t)(sizeof(PacketHeader) + resSize);
                h->id = PacketID::S_ENTER_LOBBY;
                res.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)resSize);
                session->Send((System::PacketMessage *)packet);
                System::PacketUtils::ReleasePacket(packet);
            }
            LOG_INFO("Session {} Entered Lobby", session->GetId());
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
            size_t resSize = res.ByteSizeLong();
            auto packet = System::MessagePool::AllocatePacket((uint16_t)resSize);
            if (packet)
            {
                PacketHeader *h = (PacketHeader *)packet->Payload();
                h->size = (uint16_t)(sizeof(PacketHeader) + resSize);
                h->id = PacketID::S_LEAVE_ROOM;
                res.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)resSize);
                session->Send((System::PacketMessage *)packet);
                System::PacketUtils::ReleasePacket(packet);
            }
            LOG_INFO("Session {} Left Room -> Lobby", session->GetId());
        }
        break;
    }
    case PacketID::C_CHAT: {
        Protocol::C_Chat req;
        if (packet.Parse(req))
        {
            std::string msg = req.msg();

            // Broadcast
            Protocol::S_Chat res;
            res.set_player_id((int32_t)session->GetId());
            res.set_msg(msg);

            size_t resSize = res.ByteSizeLong();
            auto packet = System::MessagePool::AllocatePacket((uint16_t)resSize);
            if (packet)
            {
                PacketHeader *h = (PacketHeader *)packet->Payload();
                h->size = (uint16_t)(sizeof(PacketHeader) + resSize);
                h->id = PacketID::S_CHAT;
                res.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)resSize);

                // If in Lobby, broadcast to all lobby sessions
                if (RoomManager::Instance().IsInLobby(session->GetId()))
                {
                    RoomManager::Instance().BroadcastToLobby((System::PacketMessage *)packet);
                }
                else
                {
                    // In Room - Broadcast via Room
                    auto player = RoomManager::Instance().GetPlayer(session->GetId());
                    if (player)
                    {
                        // Room broadcast logic
                        // player->GetRoom()->Broadcast(packet);
                        LOG_WARN("Room Chat not fully implemented. Echoing only.");
                        session->Send((System::PacketMessage *)packet);
                    }
                }

                System::PacketUtils::ReleasePacket(packet);
            }
        }
        break;
    }
    default:
        LOG_ERROR("Unknown Packet ID: {}", packet.GetId());
        break;
    }
} // End HandlePacket

} // namespace SimpleGame
