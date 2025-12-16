#pragma once

#include "GameEvents.h"
#include "Protocol.h"
#include "Protocol.pb.h"
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
        case PacketID::C_LOGIN_REQ: {
            Protocol::LoginRequest req;
            if (req.ParseFromArray(payload, (int)payloadSize))
            {
                // Map to Event
                LoginRequestEvent evt;
                evt.sessionId = session->GetId();
                evt.username = req.username();
                evt.password = req.password();
                EventBus::Instance().Publish(evt);
            }
            else
            {
                LOG_ERROR("Failed to parse C_LOGIN_REQ");
            }
            break;
        }
        case PacketID::C_MOVE: {
            Protocol::CS_Move req;
            if (req.ParseFromArray(payload, (int)payloadSize))
            {
                // Logic to handle move (Propagate to Room/Player)
                LOG_INFO("Player {} moved to ({}, {})", session->GetId(), req.x(), req.y());
            }
            break;
        }
        default:
            LOG_ERROR("Unknown Packet ID: {}", header->id);
            break;
        }
    }
};

} // namespace SimpleGame
