#pragma once

#include "System/Dispatcher/IDispatcher.h"
#include "System/ILog.h"
#include <functional>
#include <map>
#include <mutex>
#include <typeindex>
#include <vector>

namespace System {

/*
    Type-Safe Asynchronous Event Bus.
    Delegate execution to IDispatcher (Thread-Safe).
*/
class EventBus
{
public:
    static EventBus &Instance()
    {
        static EventBus instance;
        return instance;
    }

    // For Testing: Clear all listeners
    void Reset()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _listeners.clear();
    }

    // Callback signature: void(const EventType&)
    // Stored as generic void(const void*) wrapper
    using GenericCallback = std::function<void(const void *)>;

    struct Listener
    {
        IDispatcher *target;
        GenericCallback func;
    };

    template <typename EventType, typename CallbackType> void Subscribe(IDispatcher *target, CallbackType callback)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::type_index typeIdx(typeid(EventType));

        // Wrap specific callback into generic one
        GenericCallback wrapper = [callback](const void *eventPtr)
        {
            const EventType *ev = static_cast<const EventType *>(eventPtr);
            callback(*ev);
        };

        _listeners[typeIdx].push_back({target, wrapper});
        LOG_INFO("EventBus: Subscribed to {}", typeid(EventType).name());
    }

    template <typename EventType> void Publish(EventType event)
    {
        // Copy event to capture in lambda
        // Note: EventType must be CopyConstructible
        // Optimization: Use shared_ptr<Event> for large events? For now, value copy is safer.

        std::lock_guard<std::mutex> lock(_mutex);
        std::type_index typeIdx(typeid(EventType));

        auto it = _listeners.find(typeIdx);
        if (it == _listeners.end())
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
    std::mutex _mutex;
    std::map<std::type_index, std::vector<Listener>> _listeners;
};

} // namespace System
