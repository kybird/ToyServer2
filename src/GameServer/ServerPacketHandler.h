#pragma once
#include "Share/Protocol.h"
#include "System/Debug/MemoryMetrics.h"
#include "System/Dispatcher/IPacketHandler.h"
#include "System/ILog.h"
#include "System/Network/PacketUtils.h"
#include <iostream>

class ServerPacketHandler : public System::IPacketHandler
{
public:
    void HandlePacket(System::ISession *session, System::PacketMessage *packet) override
    {
        // Simple ECHO logic
        if (packet->length < sizeof(Share::PacketHeader))
            return;

        Share::PacketHeader *header = reinterpret_cast<Share::PacketHeader *>(packet->Payload());

        if (header->id == Share::PacketType::PKT_C_ECHO)
        {
            // Allocate new packet from pool
            auto response = System::PacketUtils::CreatePacket(packet->length);
            if (!response)
                return;
            std::memcpy(response->Payload(), packet->Payload(), packet->length);

            Share::PacketHeader *respHeader = reinterpret_cast<Share::PacketHeader *>(response->Payload());
            respHeader->id = Share::PacketType::PKT_S_ECHO;

            session->Send(response);

#ifdef ENABLE_DIAGNOSTICS
            // [Diagnostics] Echo 응답 카운트
            System::Debug::MemoryMetrics::Echoed.fetch_add(1, std::memory_order_relaxed);
#endif
        }
    }
};
