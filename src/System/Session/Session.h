#pragma once

#include "System/Dispatcher/IMessage.h"
#include "System/ISession.h"
#include "System/Network/RateLimiter.h" // Added (Keep this if it doesn't use boost, seemingly clean)
#include "System/Network/RecvBuffer.h"  // RecvBuffer seems to use vector, minimal deps? Check later.

#include <memory>
#include <string>
#include <vector>

namespace System {

class IDispatcher;
struct IPacketEncryption;
class PacketBroadcast;
struct SessionImpl; // Forward Declaration

/*
    High-Performance AsioSession for MMORPG Servers.
    (PIMPL Pattern Applied to hide Boost Asio Dependencies)
*/
class Session : public ISession
{
    friend class PacketBroadcast;

public:
    Session();
    // [PIMPL] socket is passed as shared_ptr<void> to erase Boost type from header
    Session(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher);
    virtual ~Session();

    // Pool Hooks
    void Reset() override;
    // [PIMPL] socket is passed as shared_ptr<void>
    void Reset(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher);
    void OnRecycle() override;

    // Graceful Shutdown
    void GracefulClose();

    // Callbacks
    void OnConnect();
    void OnDisconnect();

    // ISession Interface
    void SendPacket(const IPacket &pkt) override;
    void Close() override;
    uint64_t GetId() const override;

    void OnError(const std::string &errorMsg);

    std::thread::id GetDispatcherThreadId() const;
    std::string GetRemoteAddress() const;

    // Encryption
    void SetEncryption(std::unique_ptr<IPacketEncryption> encryption);

    // Heartbeat (Ping/Pong)
    void ConfigHeartbeat(uint32_t intervalMs, uint32_t timeoutMs, std::function<void(Session *)> pingFunc);
    void OnPong();

    // Lifetime safety
    void IncRef() override;
    void DecRef() override;
    bool CanDestroy() const override;
    bool IsConnected() const;

private:
    // [PIMPL] Internal sender for PacketBroadcast
    void SendPreSerialized(const PacketMessage *source);

private:
    std::unique_ptr<SessionImpl> _impl;
};

} // namespace System
