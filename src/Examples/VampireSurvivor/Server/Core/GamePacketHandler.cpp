#include "Core/GamePacketHandler.h"
#include "Protocol/game.pb.h"
#include "Entity/Player.h"
#include "Entity/PlayerFactory.h"
#include "Game/RoomManager.h"
#include "GameEvents.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Events/EventBus.h"
#include "System/ILog.h"
#include "System/ISession.h"
#include "System/Network/ByteBuffer.h"


namespace SimpleGame {

void GamePacketHandler::HandlePacket(System::ISession *session, System::PacketMessage *packet)
{
    // Check size
    if (packet->length < sizeof(PacketHeader))
    {
        LOG_ERROR("Packet too small");
        return;
    }

    PacketHeader *header = (PacketHeader *)packet->Payload();
    uint8_t *payload = packet->Payload() + sizeof(PacketHeader);
    size_t payloadSize = packet->length - sizeof(PacketHeader);

    switch (header->id)
    {
    case PacketID::C_LOGIN: {
        Protocol::C_Login req;
        if (req.ParseFromArray(payload, (int)payloadSize))
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
        if (req.ParseFromArray(payload, (int)payloadSize))
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
        if (req.ParseFromArray(payload, (int)payloadSize))
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
        if (req.ParseFromArray(payload, (int)payloadSize))
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
    default:
        LOG_ERROR("Unknown Packet ID: {}", header->id);
        break;
    }
}

} // namespace SimpleGame
