#pragma once

#include "System/Dispatcher/IMessage.h"
#include "System/ISession.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>
#include <string>
#include <thread>

namespace System {

class IDispatcher;
struct GatewaySessionImpl;
struct BackendSessionImpl;

/**
 * @brief Base class for all Session types.
 * Holds shared state and implements common packet sending logic.
 * Inherits from ISession.
 */
class Session : public ISession
{
    friend struct GatewaySessionImpl;
    friend struct BackendSessionImpl;

public:
    Session();
    virtual ~Session();

    // ISession Interface (Shared implementation)
    uint64_t GetId() const override;
    void Reset() override;
    bool IsConnected() const override;
    bool CanDestroy() const override;

    void IncRef() override;
    void DecRef() override;

    void SendPacket(const IPacket &pkt) override;
    void SendPacket(PacketPtr msg) override;
    void SendPreSerialized(const PacketMessage *msg) override;

    void SendReliable(const IPacket &pkt) override;
    void SendUnreliable(const IPacket &pkt) override;

    // Default implementations for optional hooks
    void OnRecycle() override
    {
    }
    void OnPong() override
    {
    }

protected:
    // Internal Enqueue logic used by all Send methods
    void EnqueueSend(PacketMessage *msg);

    // Virtual hooks to be implemented by derived classes (using PIMPL to hide implementation)
    virtual void Flush() = 0;

protected:
    // Shared State
    uint64_t _id = 0;
    IDispatcher *_dispatcher = nullptr;
    std::atomic<bool> _connected{false};
    std::atomic<int> _ioRef{0};

    // Shared Networking State (Lock-Free)
    moodycamel::ConcurrentQueue<PacketMessage *> _sendQueue;
    std::atomic<bool> _isSending{false};

    // Tracking for stats (can be moved or shared)
    std::chrono::steady_clock::time_point _lastStatTime;
};

} // namespace System
