#pragma once
#include "System/ISession.h"
#include <cstdint>
#include <memory>
#include <vector>

#include "System/Dispatcher/IMessage.h"
#include "System/PacketView.h"
#include "System/Pch.h"


namespace System {

class ISession;
struct PacketMessage;

class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    // Interface Change: PacketMessage* -> PacketView (Zero-Copy View)
    virtual void HandlePacket(ISession *session, PacketView packet) = 0;
};

} // namespace System
