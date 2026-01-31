#pragma once

#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/IPacketHandler.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace System {

class ISession;

class DispatcherImpl : public IDispatcher
{
public:
    static constexpr size_t HIGH_WATER = 5000;
    static constexpr size_t LOW_WATER = 3000;

    DispatcherImpl(std::shared_ptr<IPacketHandler> packetHandler);
    virtual ~DispatcherImpl() override;

    void Post(IMessage *message) override;

    // Main Loop processing (Called by Worker Threads)
    bool Process() override;
    void Wait(int timeoutMs) override;

    size_t GetQueueSize() const override;
    bool IsOverloaded() const override;
    bool IsRecovered() const override;

    void RegisterTimerHandler(ITimerHandler *handler) override;

    // Session-safe access
    void WithSession(uint64_t sessionId, std::function<void(SessionContext &)> callback) override;

    // Generic Task Submission
    void Push(std::function<void()> task) override;

    // Graceful Shutdown
    void Shutdown() override;

private:
    void ProcessPendingDestroys();

    // Message Handlers
    void HandlePacketMessage(IMessage *msg);
    void HandleTimerMessage(IMessage *msg);
    void HandleTimerExpiredMessage(IMessage *msg);
    void HandleTimerAddMessage(IMessage *msg);
    void HandleTimerCancelMessage(IMessage *msg);
    void HandleTimerTickMessage(IMessage *msg);
    static void HandleLambdaMessage(IMessage *msg);

    moodycamel::ConcurrentQueue<IMessage *> _messageQueue;

    std::mutex _mutex;
    std::condition_variable _cv;
    std::shared_ptr<IPacketHandler> _packetHandler;

    // [Session Registry] Maps sessionId to ISession* for safe access in WithSession
    std::unordered_map<uint64_t, ISession *> _sessions;
    std::vector<ISession *> _pendingDestroy;

    // System Handlers
    ITimerHandler *_timerHandler = nullptr;

    // [Optimization] Smart Notify using wait count
    std::atomic<int32_t> _waitingCount{0};
};

} // namespace System
