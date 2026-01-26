#pragma once

#include "System/PacketView.h"
#include "System/Session/SessionContext.h"
#include <cstdint>
#include <memory>

namespace System {

class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    // [SessionContext Refactoring] Changed from ISession* to SessionContext (value passing)
    virtual void HandlePacket(SessionContext ctx, PacketView packet) = 0;

    // Optional: Notification of disconnection to clean up game state (e.g. Lobby/Room)
    // Note: SessionContext is still valid during this call, but session is about to be destroyed
    virtual void OnSessionDisconnect(SessionContext ctx)
    {
    }
};

} // namespace System
