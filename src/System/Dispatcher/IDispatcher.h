#pragma once

#include "System/Dispatcher/IMessage.h"

namespace System {

class IDispatcher {
public:
    virtual ~IDispatcher() = default;
    virtual void Post(SystemMessage message) = 0;
    virtual void Process() = 0;
};

// Global Access Removed. Use Dependency Injection.

} // namespace System
