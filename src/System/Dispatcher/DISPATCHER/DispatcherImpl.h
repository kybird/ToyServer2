#pragma once

#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/IPacketHandler.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>
#include <vector>

static constexpr size_t HIGH_WATER = 5000;
static constexpr size_t LOW_WATER = 3000;

namespace System {

class Session;

class DispatcherImpl : public IDispatcher
{
public:
    DispatcherImpl(std::shared_ptr<IPacketHandler> packetHandler);
    virtual ~DispatcherImpl();

    void Post(IMessage *message) override;

    // Main Loop processing (Called by Worker Threads)
    bool Process() override;

    size_t GetQueueSize() const override;
    bool IsOverloaded() const override;
    bool IsRecovered() const override;

    void RegisterTimerHandler(ITimerHandler *handler) override;

    // Generic Task Submission
    void Push(std::function<void()> task) override;

private:
    void ProcessPendingDestroys();

private:
    moodycamel::ConcurrentQueue<IMessage *> _messageQueue;

    std::mutex _mutex;
    std::condition_variable _cv;
    std::shared_ptr<IPacketHandler> _packetHandler;

    // [No session registry] Dispatcher doesn't own sessions, just processes messages
    // Session lifetime is managed by Session IncRef/DecRef
    std::vector<Session *> _pendingDestroy;

    // System Handlers
    ITimerHandler *_timerHandler = nullptr;
};

} // namespace System
