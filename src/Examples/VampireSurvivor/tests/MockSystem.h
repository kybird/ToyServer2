#pragma once
#include "System/Dispatcher/IDispatcher.h"
#include "System/IFramework.h"
#include "System/ISession.h"
#include "System/ITimer.h"
#include "System/Thread/IStrand.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace System {

class MockStrand : public IStrand
{
public:
    void Post(std::function<void()> task) override
    {
        task();
    }
};

class MockDispatcher : public IDispatcher
{
public:
    void Post(IMessage *message) override
    {
    }
    bool Process() override
    {
        return true;
    }
    void Wait(int timeoutMs) override
    {
    }
    size_t GetQueueSize() const override
    {
        return 0;
    }
    bool IsOverloaded() const override
    {
        return false;
    }
    bool IsRecovered() const override
    {
        return true;
    }

    // Test helper to track calls
    struct WithSessionCall
    {
        uint64_t sessionId;
    };
    std::vector<WithSessionCall> calls;

    void RegisterSession(uint64_t id, ISession *session)
    {
        _sessions[id] = session;
    }

    void WithSession(uint64_t sessionId, std::function<void(SessionContext &)> callback) override
    {
        calls.push_back({sessionId});
        // Note: Cannot call the lambda because SessionContext has a private constructor.
        // We just track that the attempt was made.
    }

    void RegisterTimerHandler(ITimerHandler *handler) override
    {
    }
    void Push(std::function<void()> task) override
    {
        task();
    }
    void Shutdown() override
    {
    }

private:
    std::unordered_map<uint64_t, ISession *> _sessions;
};

class MockTimer : public ITimer
{
public:
    TimerHandle SetTimer(uint32_t timerId, uint32_t delayMs, ITimerListener *listener, void *pParam = nullptr) override
    {
        return 0;
    }
    TimerHandle SetTimer(
        uint32_t timerId, uint32_t delayMs, std::weak_ptr<ITimerListener> listener, void *pParam = nullptr
    ) override
    {
        return 0;
    }
    TimerHandle
    SetInterval(uint32_t timerId, uint32_t intervalMs, ITimerListener *listener, void *pParam = nullptr) override
    {
        return ++_counter;
    }
    TimerHandle SetInterval(
        uint32_t timerId, uint32_t intervalMs, std::weak_ptr<ITimerListener> listener, void *pParam = nullptr
    ) override
    {
        return ++_counter;
    }
    void CancelTimer(TimerHandle handle) override
    {
    }
    void Unregister(ITimerListener *listener) override
    {
    }

private:
    TimerHandle _counter = 0;
};

class MockFramework : public IFramework
{
public:
    MockFramework()
    {
        _mockDispatcher = std::make_shared<MockDispatcher>();
        _timer = std::make_shared<MockTimer>();
    }
    bool Init(std::shared_ptr<IConfig> config, std::shared_ptr<IPacketHandler> packetHandler) override
    {
        return true;
    }
    void Run() override
    {
    }
    void Stop() override
    {
    }
    void Join() override
    {
    }
    std::shared_ptr<ITimer> GetTimer() const override
    {
        return _timer;
    }
    std::shared_ptr<IStrand> CreateStrand() override
    {
        return std::make_shared<MockStrand>();
    }
    size_t GetDispatcherQueueSize() const override
    {
        return 0;
    }
    std::shared_ptr<IDispatcher> GetDispatcher() const override
    {
        return _mockDispatcher;
    }
    std::shared_ptr<IDatabase> GetDatabase() const override
    {
        return nullptr;
    }
    std::shared_ptr<ThreadPool> GetThreadPool() const override
    {
        return nullptr;
    }
    std::shared_ptr<ICommandConsole> GetCommandConsole() const override
    {
        return nullptr;
    }
    std::shared_ptr<INetwork> GetNetwork() const override
    {
        return nullptr;
    }

    std::shared_ptr<MockDispatcher> GetMockDispatcher()
    {
        return _mockDispatcher;
    }

private:
    std::shared_ptr<MockDispatcher> _mockDispatcher;
    std::shared_ptr<ITimer> _timer;
};

} // namespace System
