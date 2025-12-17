#pragma once

#include "GameEvents.h"
#include "Protocol.h"
#include "../../Protocol/game.pb.h"
#include "System/Dispatcher/IPacketHandler.h"
#include "System/Events/EventBus.h"
#include "System/ILog.h"
#include "System/Memory/ObjectPool.h"
#include "System/Network/ByteBuffer.h"

using namespace System;

namespace SimpleGame {

class GamePacketHandler : public IPacketHandler
{
public:
    void HandlePacket(ISession *session, PacketMessage *packet) override
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
                evt.username = req.username();
                evt.password = req.password();
                EventBus::Instance().Publish(evt);
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
                // Logic to handle move (Client sends input direction)
                // TODO: EventBus::Instance().Publish(MoveEvent{...});
                LOG_INFO("Player {} Input: ({}, {})", session->GetId(), req.dir_x(), req.dir_y());
            }
            break;
        }
        case PacketID::C_CREATE_ROOM:
        case PacketID::C_JOIN_ROOM:
        case PacketID::C_USE_SKILL:
        case PacketID::C_SELECT_LEVEL_UP:
            LOG_WARN("Packet ID {} not implemented yet", header->id);
            break;
        default:
            LOG_ERROR("Unknown Packet ID: {}", header->id);
            break;
        }
    }
};

} // namespace SimpleGame
