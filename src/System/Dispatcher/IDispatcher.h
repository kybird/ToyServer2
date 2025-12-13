#pragma once

#include "System/Dispatcher/IMessage.h"

namespace System {

class IDispatcher
{
public:
    virtual ~IDispatcher() = default;
    virtual void Post(SystemMessage message) = 0;
    virtual bool Process() = 0;
    virtual size_t GetQueueSize() const = 0;
    virtual bool IsOverloaded() const = 0;
};

// Global Access Removed. Use Dependency Injection.

} // namespace System
