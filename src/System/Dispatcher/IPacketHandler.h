#pragma once
#include "System/ISession.h"
#include <cstdint>
#include <memory>
#include <vector>

#include "System/Dispatcher/IMessage.h"
namespace System {

class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    // Zero-copy: Packet data is PacketMessage* (Single Allocation)
    // Session is pre-looked-up ISession (Raw Pointer)
    virtual void HandlePacket(ISession *session, PacketMessage *packet) = 0;
};

} // namespace System
