#pragma once
#include "Share/Protocol.h"
#include "System/Dispatcher/IPacketHandler.h"
#include "System/ILog.h"
#include <iostream>


class ServerPacketHandler : public System::IPacketHandler {
public:
    void HandlePacket(std::shared_ptr<System::ISession> session,
                      std::shared_ptr<std::vector<uint8_t>> packet) override {
        // Simple ECHO logic
        if (packet->size() < sizeof(Share::PacketHeader))
            return;

        Share::PacketHeader *header = reinterpret_cast<Share::PacketHeader *>(packet->data());

        if (header->id == Share::PacketType::PKT_C_ECHO) {
            // Re-use packet buffer for response?
            // Since we need to modify ID, and packet is shared_ptr (const-ish usage usually),
            // we should make a copy for response to be safe.
            // Or if we know we are the only owner (Dispatcher passes shared_ptr, but it came from PacketPool).

            // Let's create a new vector for response to be safe and clean.
            // (Optimization possibility: PacketPool::Allocate copy)

            auto response = std::make_shared<std::vector<uint8_t>>(*packet);
            Share::PacketHeader *respHeader = reinterpret_cast<Share::PacketHeader *>(response->data());
            respHeader->id = Share::PacketType::PKT_S_ECHO;

            session->Send(response);
        }
    }
};
