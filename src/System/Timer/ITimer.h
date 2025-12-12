#pragma once
#include <chrono>
#include <functional>

namespace System {

class ITimer {
public:
    virtual ~ITimer() = default;

    // Schedule a callback to be executed after a delay
    virtual void SetTimer(std::chrono::milliseconds delay, std::function<void()> callback) = 0;

    // Schedule a callback to be executed repeatedly at a fixed interval
    virtual void SetInterval(std::chrono::milliseconds interval, std::function<void()> callback) = 0;
};

// Global Access - REMOVED
// ITimer &GetTimer();
// void SetGlobalTimer(std::shared_ptr<ITimer> timer);

} // namespace System
