#pragma once
#include "System/Dispatcher/SystemMessages.h"
#include "System/ITimer.h"
#include "System/Timer/TimingWheel.h"
#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>

namespace System {

class IDispatcher;

class TimerImpl : public ITimer, public ITimerHandler, public std::enable_shared_from_this<TimerImpl>
{
public:
    TimerImpl(boost::asio::io_context &ioContext, IDispatcher *dispatcher);
    virtual ~TimerImpl() override;

    // ITimer Interface (Thread-Safe Producers)
    TimerHandle SetTimer(uint32_t timerId, uint32_t delayMs, ITimerListener *listener, void *pParam = nullptr) override;
    TimerHandle SetTimer(
        uint32_t timerId, uint32_t delayMs, std::weak_ptr<ITimerListener> listener, void *pParam = nullptr
    ) override;

    TimerHandle
    SetInterval(uint32_t timerId, uint32_t intervalMs, ITimerListener *listener, void *pParam = nullptr) override;
    TimerHandle SetInterval(
        uint32_t timerId, uint32_t intervalMs, std::weak_ptr<ITimerListener> listener, void *pParam = nullptr
    ) override;

    void CancelTimer(TimerHandle handle) override;
    void Unregister(ITimerListener *listener) override;

    // ITimerHandler Interface (Dispatcher Thread Consumers)
    void OnTimerExpired(uint64_t timerId) override;
    void OnTimerAdd(TimerAddMessage *msg) override;
    void OnTimerCancel(TimerCancelMessage *msg) override;

private:
    // Helper to actually schedule ASIO (Dispatcher Thread)

    // Internal ID Gen (Atomic for Thread Safety of SetTimer return value)
    std::atomic<uint64_t> _nextTimerId{1};

    // State (Dispatcher Thread ONLY)

    std::unordered_map<uint64_t, std::shared_ptr<TimingWheel::Node>> _timers;
    std::unordered_multimap<void *, uint64_t> _listenerMap; // For Unregister

    TimingWheel _wheel;
    std::shared_ptr<boost::asio::steady_timer> _tickTimer;
    std::atomic<uint32_t> _currentTick{0}; // Keep track of logical ticks

    // Constants
    static constexpr uint32_t TICK_INTERVAL_MS = 10;

    // Tick Scheduler
    void ScheduleTick();
    void OnTick(TimerTickMessage *msg) override;

    // Dependencies
    boost::asio::io_context &_ioContext;
    IDispatcher *_dispatcher;

    // We allocate steady_timer via make_shared.
}; // For Unregister(IListener)
} // namespace System
