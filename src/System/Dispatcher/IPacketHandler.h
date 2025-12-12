#pragma once
#include "System/Network/ISession.h"
#include <cstdint>
#include <memory>
#include <vector>


namespace System {

class IPacketHandler {
public:
    virtual ~IPacketHandler() = default;

    // Zero-copy: Packet data is shared_ptr
    // Session is pre-looked-up ISession
    virtual void HandlePacket(std::shared_ptr<ISession> session, std::shared_ptr<std::vector<uint8_t>> packet) = 0;
};

} // namespace System
