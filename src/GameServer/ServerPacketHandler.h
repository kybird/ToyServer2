#pragma once
#include "Share/Protocol.h"
#include "System/Dispatcher/IPacketHandler.h"
#include "System/ILog.h"
#include "System/Memory/PacketPool.h"
#include <iostream>

class ServerPacketHandler : public System::IPacketHandler
{
public:
    void HandlePacket(System::ISession *session, boost::intrusive_ptr<System::Packet> packet) override
    {
        // Simple ECHO logic
        if (packet->size < sizeof(Share::PacketHeader))
            return;

        Share::PacketHeader *header = reinterpret_cast<Share::PacketHeader *>(packet->data());

        if (header->id == Share::PacketType::PKT_C_ECHO)
        {

            // Allocate new packet from pool
            auto response = System::PacketPool::Allocate(packet->size);
            response->assign(packet->data(), packet->data() + packet->size);

            Share::PacketHeader *respHeader = reinterpret_cast<Share::PacketHeader *>(response->data());
            respHeader->id = Share::PacketType::PKT_S_ECHO;

            session->Send(std::move(response));
        }
    }
};
