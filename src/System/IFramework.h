#pragma once

#include "System/Events/EventBus.h"
#include <memory>
#include <string>

namespace System {

class IConfig;
class IDispatcher;
class ITimer;
class IPacketHandler;
class IStrand;
class IDatabase;
class IDatabase;
class ThreadPool;
class INetwork;

class IFramework
{
public:
    virtual ~IFramework() = default;

    // Static Factory Method
    static std::unique_ptr<IFramework> Create();

    // Core Lifecycle
    virtual bool Init(std::shared_ptr<IConfig> config, std::shared_ptr<IPacketHandler> packetHandler) = 0;
    virtual void Run() = 0;
    virtual void Stop() = 0;
    virtual void Join() = 0; // [New] Wait for cleanup

    // Accessors
    // Accessors
    virtual std::shared_ptr<ITimer> GetTimer() const = 0;
    virtual std::shared_ptr<IStrand> CreateStrand() = 0;
    virtual size_t GetDispatcherQueueSize() const = 0;
    virtual std::shared_ptr<IDispatcher> GetDispatcher() const = 0;
    virtual std::shared_ptr<IDatabase> GetDatabase() const = 0;
    virtual std::shared_ptr<ThreadPool> GetThreadPool() const = 0;

    // Command Console Accessor
    virtual std::shared_ptr<class ICommandConsole> GetCommandConsole() const = 0;

    // Network Accessor
    virtual std::shared_ptr<INetwork> GetNetwork() const = 0;

    // Event Subscription Helper (Hides IDispatcher)
    template <typename EventType, typename CallbackType> void Subscribe(CallbackType callback)
    {
        // Default: Subscribe to Main Logic Dispatcher
        System::EventBus::Instance().Subscribe<EventType>(GetDispatcher().get(), callback);
    }
};

} // namespace System
