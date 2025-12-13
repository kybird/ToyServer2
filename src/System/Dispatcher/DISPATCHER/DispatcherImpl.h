#pragma once

#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/IPacketHandler.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>

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

private:
    moodycamel::ConcurrentQueue<SystemMessage> _messageQueue;
    std::shared_ptr<IPacketHandler> _packetHandler;
};

} // namespace System
