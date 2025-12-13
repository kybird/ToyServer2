#pragma once
#include "System/Network/ISession.h"
#include <cstdint>
#include <memory>
#include <vector>

#include "System/Network/Packet.h"
#include <boost/smart_ptr/intrusive_ptr.hpp>

namespace System {

class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    // Zero-copy: Packet data is boost::intrusive_ptr<Packet>
    // Session is pre-looked-up ISession (Raw Pointer)
    virtual void HandlePacket(ISession *session, boost::intrusive_ptr<Packet> packet) = 0;
};

} // namespace System
