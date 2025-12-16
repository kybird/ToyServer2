#pragma once

#include "System/Dispatcher/IMessage.h"

namespace System {
struct ITimerHandler;

class IDispatcher
{
public:
    virtual ~IDispatcher() = default;
    virtual void Post(IMessage *message) = 0;
    virtual bool Process() = 0;
    virtual size_t GetQueueSize() const = 0;
    virtual bool IsOverloaded() const = 0;
    virtual bool IsRecovered() const = 0;

    // System Handlers
    virtual void RegisterTimerHandler(ITimerHandler *handler) = 0;
};

// Global Access Removed. Use Dependency Injection.

} // namespace System
