#pragma once

#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/IPacketHandler.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>
#include <vector>

static constexpr size_t HIGH_WATER = 5000;
static constexpr size_t LOW_WATER = 3000;

namespace System {

class AsioSession;

class DispatcherImpl : public IDispatcher
{
public:
    DispatcherImpl(std::shared_ptr<IPacketHandler> packetHandler);
    virtual ~DispatcherImpl();

    void Post(IMessage *message) override;

    // Main Loop processing (Called by Worker Threads)
    bool Process() override;

    size_t GetQueueSize() const override
    {
        return _messageQueue.size_approx();
    }

    bool IsOverloaded() const override
    {
        return _messageQueue.size_approx() > HIGH_WATER;
    }

    bool IsRecovered() const override
    {
        return _messageQueue.size_approx() < LOW_WATER;
    }

private:
    void ProcessPendingDestroys();

private:
    moodycamel::ConcurrentQueue<IMessage *> _messageQueue;
    std::shared_ptr<IPacketHandler> _packetHandler;

    // [No session registry] Dispatcher doesn't own sessions, just processes messages
    // Session lifetime is managed by AsioSession IncRef/DecRef
    std::vector<AsioSession *> _pendingDestroy;
};

} // namespace System
