#pragma once

#include "System/Dispatcher/IDispatcher.h"
#include "System/ILog.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace System {

/*
    Type-Safe Asynchronous Event Bus.
    Delegate execution to IDispatcher (Thread-Safe).
    Uses Copy-On-Write (COW) pattern to ensure Lock-Free Publish.
*/
class EventBus
{
public:
    static EventBus &Instance()
    {
        static EventBus instance;
        return instance;
    }

    // Callback signature: void(const EventType&)
    // Stored as generic void(const void*) wrapper
    using GenericCallback = std::function<void(const void *)>;

    struct Listener
    {
        IDispatcher *target;
        GenericCallback func;
    };

    using ListenerMap = std::unordered_map<std::type_index, std::vector<Listener>>;

    EventBus()
    {
        // Initialize with an empty map
        _listeners.store(std::make_shared<ListenerMap>());
    }

    // For Testing: Clear all listeners
    void Reset()
    {
        std::lock_guard<std::mutex> lock(_writeMutex);
        _listeners.store(std::make_shared<ListenerMap>());
    }

    template <typename EventType, typename CallbackType> void Subscribe(IDispatcher *target, CallbackType callback)
    {
        std::lock_guard<std::mutex> lock(_writeMutex);
        std::type_index typeIdx(typeid(EventType));

        // Wrap specific callback into generic one
        GenericCallback wrapper = [callback](const void *eventPtr)
        {
            const EventType *ev = static_cast<const EventType *>(eventPtr);
            callback(*ev);
        };

        // 1. Read current map
        std::shared_ptr<ListenerMap> currentMap = _listeners.load();

        // 2. Copy current map (Copy-On-Write)
        auto newMap = std::make_shared<ListenerMap>(*currentMap);

        // 3. Modify the new map
        (*newMap)[typeIdx].push_back({target, wrapper});

        // 4. Atomically swap the pointer
        _listeners.store(newMap);

        LOG_INFO("EventBus: Subscribed to {}", typeid(EventType).name());
    }

    template <typename EventType> void Publish(EventType event)
    {
        // Lock-Free Read using std::atomic<std::shared_ptr<T>> (C++20 feature)
        std::shared_ptr<ListenerMap> currentMap = _listeners.load();

        std::type_index typeIdx(typeid(EventType));

        auto it = currentMap->find(typeIdx);
        if (it == currentMap->end())
            return;

        for (const auto &listener : it->second)
        {
            if (listener.target)
            {
                // Async Dispatch: Push job to target thread
                // Lambda captures event by value
                listener.target->Push(
                    [event, func = listener.func]()
                    {
                        func(&event);
                    }
                );
            }
            else
            {
                // Immediate Execution (Use with caution!)
                listener.func(&event);
            }
        }
    }

private:
    std::mutex _writeMutex; // Used ONLY for Subscribe/Reset to serialize writes
    std::atomic<std::shared_ptr<ListenerMap>> _listeners;
};

} // namespace System
