#pragma once
#include "System/Packet/PacketPtr.h" // [New]
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace System {

class IPacket;
struct PacketMessage;

class ISession
{
public:
    virtual ~ISession() = default;

    // [User API] Send a packet (PacketMessage created internally)
    virtual void SendPacket(const IPacket &pkt) = 0;
    virtual void SendPacket(PacketPtr msg) = 0; // [New] RAII Async Safe

    // [System API] Send pre-serialized message (Broadcast optimization)
    virtual void SendPreSerialized(const PacketMessage *msg) = 0;

    virtual void Close() = 0;
    virtual uint64_t GetId() const = 0;
    virtual void Reset() = 0;
    virtual bool CanDestroy() const = 0;
    virtual void OnRecycle()
    {
    }

    // Heartbeat Support
    virtual void OnPong()
    {
    }

    // Connection Lifecycle
    virtual void OnConnect() = 0;
    virtual void OnDisconnect() = 0;
    virtual bool IsConnected() const = 0;

    // [Lifetime] Reference counting for async message queue safety
    virtual void IncRef() = 0;
    virtual void DecRef() = 0;
};

} // namespace System
