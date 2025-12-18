#pragma once
#include "Share/Protocol.h"
#include "System/Debug/MemoryMetrics.h"
#include "System/Dispatcher/IPacketHandler.h"
#include "System/ILog.h"
#include "System/Network/PacketUtils.h"
#include "System/PacketView.h" // Added
#include <iostream>

class ServerPacketHandler : public System::IPacketHandler
{
public:
    void HandlePacket(System::ISession *session, System::PacketView packet) override
    {
        // Simple ECHO logic
        // PacketView provides stripped payload and ID.

        if (packet.GetId() == Share::PacketType::PKT_C_ECHO)
        {
            // Calculate total size: Header + Body
            uint16_t bodyLen = (uint16_t)packet.GetLength();
            uint16_t totalSize = (uint16_t)(sizeof(Share::PacketHeader) + bodyLen);

            // Allocate new packet from pool
            auto response = System::PacketUtils::CreatePacket(totalSize);
            if (!response)
                return;

            // Construct Header
            Share::PacketHeader *respHeader = reinterpret_cast<Share::PacketHeader *>(response->Payload());
            respHeader->size = totalSize;
            respHeader->id = Share::PacketType::PKT_S_ECHO;

            // Copy Payload
            std::memcpy(response->Payload() + sizeof(Share::PacketHeader), packet.GetPayload(), bodyLen);

            session->Send(response);
            System::PacketUtils::ReleasePacket(response); // Release after Send (since CreatePacket was used)

#ifdef ENABLE_DIAGNOSTICS
            // [Diagnostics] Echo 응답 카운트
            System::Debug::MemoryMetrics::Echoed.fetch_add(1, std::memory_order_relaxed);
#endif
        }
    }
};
