#pragma once

#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/IPacketHandler.h"
#include "System/Network/ASIO/SessionPool.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>
#include <unordered_map>
#include <vector>

namespace System {

class DispatcherImpl : public IDispatcher
{
public:
    DispatcherImpl(std::shared_ptr<IPacketHandler> packetHandler);
    virtual ~DispatcherImpl();

    void Post(SystemMessage message) override;

    // Main Loop processing (Called by Worker Threads)
    bool Process() override;

    size_t GetQueueSize() const override
    {
        return _messageQueue.size_approx();
    }

    bool IsOverloaded() const override
    {
        // Simple Threshold
        return _messageQueue.size_approx() > 5000;
    }

private:
    void ProcessPendingDestroys();

private:
    moodycamel::ConcurrentQueue<SystemMessage> _messageQueue;
    std::shared_ptr<IPacketHandler> _packetHandler;

    // [Thread-Local Session Registry]
    // Dispatcher owns the sessions. No global lock.
    std::unordered_map<uint64_t, ISession *> _sessionRegistry;
    std::vector<ISession *> _pendingDestroy;
};

} // namespace System
