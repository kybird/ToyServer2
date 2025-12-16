#include "System/Timer/TimerImpl.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Dispatcher/SystemMessages.h"
#include "System/Pch.h"

namespace System {

TimerImpl::TimerImpl(boost::asio::io_context &ioContext, IDispatcher *dispatcher)
    : _ioContext(ioContext), _dispatcher(dispatcher)
{
    _dispatcher->RegisterTimerHandler(this);

    // Initialize Tick Timer
    _tickTimer = std::make_shared<boost::asio::steady_timer>(_ioContext);
    ScheduleTick();
}

TimerImpl::~TimerImpl()
{
}

// =========================================================================
// ITimer Interface (Producers - Thread Safe)
// =========================================================================

ITimer::TimerHandle TimerImpl::SetTimer(uint32_t timerId, uint32_t delayMs, ITimerListener *listener, void *pParam)
{
    uint64_t id = _nextTimerId++;
    auto msg = MessagePool::AllocateTimerAdd();
    if (msg)
    {
        msg->timerId = id;
        msg->logicTimerId = timerId;
        msg->intervalMs = delayMs;
        msg->isInterval = false;
        msg->listener = listener;
        msg->pParam = pParam;
        msg->weakListener.reset();
        _dispatcher->Post(msg);
    }
    return id;
}

ITimer::TimerHandle
TimerImpl::SetTimer(uint32_t timerId, uint32_t delayMs, std::weak_ptr<ITimerListener> listener, void *pParam)
{
    uint64_t id = _nextTimerId++;
    auto msg = MessagePool::AllocateTimerAdd();
    if (msg)
    {
        msg->timerId = id;
        msg->logicTimerId = timerId;
        msg->intervalMs = delayMs;
        msg->isInterval = false;
        msg->pParam = pParam;

        auto ptr = listener.lock();
        msg->listener = ptr.get();
        msg->weakListener = listener;

        _dispatcher->Post(msg);
    }
    return id;
}

ITimer::TimerHandle
TimerImpl::SetInterval(uint32_t timerId, uint32_t intervalMs, ITimerListener *listener, void *pParam)
{
    uint64_t id = _nextTimerId++;
    auto msg = MessagePool::AllocateTimerAdd();
    if (msg)
    {
        msg->timerId = id;
        msg->logicTimerId = timerId;
        msg->intervalMs = intervalMs;
        msg->isInterval = true;
        msg->listener = listener;
        msg->pParam = pParam;
        msg->weakListener.reset();
        _dispatcher->Post(msg);
    }
    return id;
}

ITimer::TimerHandle
TimerImpl::SetInterval(uint32_t timerId, uint32_t intervalMs, std::weak_ptr<ITimerListener> listener, void *pParam)
{
    uint64_t id = _nextTimerId++;
    auto msg = MessagePool::AllocateTimerAdd();
    if (msg)
    {
        msg->timerId = id;
        msg->logicTimerId = timerId;
        msg->intervalMs = intervalMs;
        msg->isInterval = true;
        msg->pParam = pParam;

        auto ptr = listener.lock();
        msg->listener = ptr.get();
        msg->weakListener = listener;

        _dispatcher->Post(msg);
    }
    return id;
}

void TimerImpl::CancelTimer(TimerHandle handle)
{
    auto msg = MessagePool::AllocateTimerCancel();
    if (msg)
    {
        msg->timerId = handle;
        msg->listener = nullptr;
        _dispatcher->Post(msg);
    }
}

void TimerImpl::Unregister(ITimerListener *listener)
{
    auto msg = MessagePool::AllocateTimerCancel();
    if (msg)
    {
        msg->timerId = 0;
        msg->listener = listener;
        _dispatcher->Post(msg);
    }
}

// =========================================================================
// ITimerHandler Interface (Consumers - Main/Logic Thread ONLY)
// =========================================================================

void TimerImpl::OnTimerAdd(TimerAddMessage *msg)
{
    auto node = std::make_shared<TimingWheel::Node>();
    node->id = msg->timerId;
    node->logicTimerId = msg->logicTimerId;
    node->pParam = msg->pParam;

    // Calculate Ticks
    // Minimum 1 tick
    uint32_t ticks = msg->intervalMs / TICK_INTERVAL_MS;
    if (ticks == 0)
        ticks = 1;

    node->intervalTick = msg->isInterval ? ticks : 0;
    node->expiryTick = _currentTick + ticks;

    node->rawListener = (ITimerListener *)msg->listener;

    if (!msg->weakListener.expired())
    {
        node->useWeak = true;
        node->weakListener = std::static_pointer_cast<ITimerListener>(msg->weakListener.lock());
    }
    else
    {
        node->useWeak = false;
    }

    // Store in Map and Wheel
    _timers[node->id] = node;
    _listenerMap.insert({node->rawListener, node->id});

    _wheel.Add(node);
}

void TimerImpl::OnTimerCancel(TimerCancelMessage *msg)
{
    if (msg->timerId != 0)
    {
        auto it = _timers.find(msg->timerId);
        if (it != _timers.end())
        {
            auto node = it->second;
            // _wheel.Remove(node); // O(N) or O(L) removed. Use Soft Delete O(1)
            node->cancelled = true;
            // We can leave it in Wheel, it will be ignored on expiry.
            // But we SHOULD remove from internal maps immediately so it can be re-used or not found.
            // If we remove from _timers map, OnTimerExpired won't find it!
            // Wait, OnTick calls OnTimerExpired(timerId).
            // Data flow:
            // 1. Tick -> Wheel -> Popped Node
            // 2. Node -> timerId
            // 3. OnTimerExpired(timerId) -> Loopup _timers[timerId]
            // If we erase from _timers now, OnTimerExpired will return early "if (it == _timers.end()) return;"
            // So we don't strictly need "cancelled" flag if we remove from map?
            // YES. If removed from map, OnTimerExpired returns early.
            // But the Node kept alive by Wheel shared_ptr until expiry.
            // That is fine. "Lazy free" of Node memory.
            // So we just clear maps. The Node in wheel is zombie.
            // When it expires, we lookup ID, fail, and drop Node.
            // Perfect.

            // Clean Maps
            auto range = _listenerMap.equal_range(node->rawListener);
            for (auto mapIt = range.first; mapIt != range.second;)
            {
                if (mapIt->second == msg->timerId)
                    mapIt = _listenerMap.erase(mapIt);
                else
                    ++mapIt;
            }
            _timers.erase(it);
        }
    }
    else if (msg->listener != nullptr)
    {
        // Cancel All
        ITimerListener *target = (ITimerListener *)msg->listener;
        auto range = _listenerMap.equal_range(target);

        // Collect to avoid iterator invalidation during linear scan if needed?
        // Map allows erasure efficiently.
        // We need to remove from Wheel too.
        std::vector<uint64_t> idsToRemove;
        for (auto it = range.first; it != range.second; ++it)
        {
            idsToRemove.push_back(it->second);
        }

        for (uint64_t tid : idsToRemove)
        {
            auto it = _timers.find(tid);
            if (it != _timers.end())
            {
                // _wheel.Remove(it->second); // Zombie mode
                _timers.erase(it);
            }
        }
        _listenerMap.erase(target);
    }
}

void TimerImpl::OnTimerExpired(uint64_t timerId)
{
    // Triggered by OnTick loop for each expired node
    auto it = _timers.find(timerId);
    if (it == _timers.end())
        return;

    auto node = it->second;
    if (node->cancelled)
    {
        // Lazy cleanup
        // Remove from listener map
        auto range = _listenerMap.equal_range(node->rawListener);
        for (auto lit = range.first; lit != range.second;)
        {
            if (lit->second == timerId)
                lit = _listenerMap.erase(lit);
            else
                ++lit;
        }
        _timers.erase(it);
        return;
    }

    // Call Listener
    if (node->useWeak)
    {
        if (auto ptr = node->weakListener.lock())
        {
            ptr->OnTimer(node->logicTimerId, node->pParam);
        }
        else
        {
            // Dead, cleanup
            _wheel.Remove(node); // Likely already removed by Advance() unless updated?
                                 // Actually Advance() removed it from slot. We just need to remove from maps.
                                 // But if it's Interval, we might need to re-add?
                                 // Let's handle return codes in OnTick loop or here?
                                 // If dead, we shouldn't re-schedule.
                                 // Cleanup maps.
            auto range = _listenerMap.equal_range(node->rawListener);
            for (auto lit = range.first; lit != range.second;)
            {
                if (lit->second == timerId)
                    lit = _listenerMap.erase(lit);
                else
                    ++lit;
            }
            _timers.erase(it);
            return;
        }
    }
    else
    {
        if (node->rawListener) // Raw pointer safety is user responsibility unless Unregistered
            node->rawListener->OnTimer(node->logicTimerId, node->pParam);
    }

    // Reschedule if Interval
    if (node->intervalTick > 0)
    {
        node->expiryTick += node->intervalTick;
        // Optimization: if expiryTick <= currentTick (execution took too long),
        // we should skip or fire immediately?
        // For game logic, we usually want to catch up or skip.
        // Let's Add back to wheel.
        _wheel.Add(node);
    }
    else
    {
        // Cleanup One Shot
        auto range = _listenerMap.equal_range(node->rawListener);
        for (auto lit = range.first; lit != range.second;)
        {
            if (lit->second == timerId)
                lit = _listenerMap.erase(lit);
            else
                ++lit;
        }
        _timers.erase(it);
    }
}

void TimerImpl::OnTick(TimerTickMessage *msg)
{
    // Advance Tick
    _currentTick++;

    std::vector<std::shared_ptr<TimingWheel::Node>> expired;
    _wheel.Advance(_currentTick, expired);

    for (auto &node : expired)
    {
        // Don't call OnTimerExpired recursively through Dispatcher Post if we can call direct.
        // We are on Logic Thread. We CAN call OnTimerExpired directly.
        // But OnTimerExpired looks up ID in Map.
        // We have the Node* here.
        // Optimization: Refactor OnTimerExpired to take Node*?
        // Or just call it by ID. It's O(1) map lookup. Safe.
        OnTimerExpired(node->id);
    }

    ScheduleTick();
}

void TimerImpl::ScheduleTick()
{
    // [Fix Drift] Interval Schedule: Previous Expiry + Interval
    // If first run, expiry is now + interval.
    // If subsequent, expiry is expiry() + interval.

    // Check if expiry is in past (initial)
    if (_tickTimer->expiry() < std::chrono::steady_clock::now())
    {
        _tickTimer->expires_after(std::chrono::milliseconds(TICK_INTERVAL_MS));
    }
    else
    {
        _tickTimer->expires_at(_tickTimer->expiry() + std::chrono::milliseconds(TICK_INTERVAL_MS));
    }

    IDispatcher *dispatch = _dispatcher;

    _tickTimer->async_wait(
        [dispatch](const boost::system::error_code &ec)
        {
            if (!ec)
            {
                auto msg = MessagePool::AllocateTimerTick();
                if (msg)
                {
                    dispatch->Post(msg);
                }
            }
        }
    );
}

} // namespace System
