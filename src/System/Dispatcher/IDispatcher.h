#pragma once

#include "System/Dispatcher/IMessage.h"
#include <functional>

namespace System {
struct ITimerHandler;

class SessionContext;

class IDispatcher
{
public:
    virtual ~IDispatcher() = default;
    virtual void Post(IMessage *message) = 0;
    virtual bool Process() = 0;
    virtual void Wait(int timeoutMs) = 0;
    virtual size_t GetQueueSize() const = 0;
    virtual bool IsOverloaded() const = 0;
    virtual bool IsRecovered() const = 0;

    // Session-safe access
    virtual void WithSession(uint64_t sessionId, std::function<void(SessionContext &)> callback) = 0;

    // System Handlers
    // System Handlers
    virtual void RegisterTimerHandler(ITimerHandler *handler) = 0;

    // Generic Task Submission
    virtual void Push(std::function<void()> task) = 0;
};

// Global Access Removed. Use Dependency Injection.

} // namespace System
