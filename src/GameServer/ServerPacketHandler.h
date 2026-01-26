#pragma once
#include "Share/EchoPacket.h"
#include "Share/Protocol.h"
#include "System/Debug/MemoryMetrics.h"
#include "System/Dispatcher/IPacketHandler.h"
#include "System/ILog.h"
#include "System/PacketView.h"
#include <iostream>

class ServerPacketHandler : public System::IPacketHandler
{
public:
    void HandlePacket(System::SessionContext ctx, System::PacketView packet) override
    {
        // Simple ECHO logic
        // PacketView provides stripped payload and ID.

        if (packet.GetId() == Share::PacketType::PKT_C_ECHO)
        {
            // Echo Response using EchoPacket (Zero-copy payload view)
            Share::EchoPacket response(std::span<const uint8_t>(packet.GetPayload(), packet.GetLength()));
            ctx.Send(response);

#ifdef ENABLE_DIAGNOSTICS
            // [Diagnostics] Echo 응답 카운트
            System::Debug::MemoryMetrics::Echoed.fetch_add(1, std::memory_order_relaxed);
#endif
        }
    }
};
