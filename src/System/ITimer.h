#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace System {

// Schedule a callback to be executed after a delay
// [Observer Pattern for Thread-Safe Handling]
// Standalone interface for Forward Declaration support
struct ITimerListener
{
    virtual ~ITimerListener() = default;
    virtual void OnTimer(uint32_t timerId, void *pParam) = 0;
};

class ITimer
{
public:
    virtual ~ITimer() = default;

    using TimerHandle = uint64_t;

    // Set a one-shot timer (Raw Pointer - Fast, Manual Lifetime)
    // Returns: Unique TimerHandle to use for cancellation
    virtual TimerHandle
    SetTimer(uint32_t timerId, uint32_t delayMs, ITimerListener *listener, void *pParam = nullptr) = 0;

    // Set a one-shot timer (Weak Pointer - Safe, Auto Cancel on Destroy)
    virtual TimerHandle
    SetTimer(uint32_t timerId, uint32_t delayMs, std::weak_ptr<ITimerListener> listener, void *pParam = nullptr) = 0;

    // Set a repeating interval timer (Raw Pointer)
    // Returns: Unique TimerHandle to use for cancellation
    virtual TimerHandle
    SetInterval(uint32_t timerId, uint32_t intervalMs, ITimerListener *listener, void *pParam = nullptr) = 0;

    // Set a repeating interval timer (Weak Pointer)
    virtual TimerHandle SetInterval(
        uint32_t timerId, uint32_t intervalMs, std::weak_ptr<ITimerListener> listener, void *pParam = nullptr
    ) = 0;

    // Cancel a specific timer by Handle
    virtual void CancelTimer(TimerHandle handle) = 0;

    // Unregister all timers associated with this listener
    // Crucial for safety when an object is destroyed
    virtual void Unregister(ITimerListener *listener) = 0;
};

} // namespace System
