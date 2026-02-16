#pragma once

#include "System/ISession.h"
#include "System/Pch.h"
#include "System/Session/BackendSession.h"
#include "System/Session/GatewaySession.h"
#include "System/Session/UDPSession.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>

#include <atomic>
#include <thread>

namespace System {

template <typename T> class SessionPoolBase
{
public:
    static constexpr double GROWTH_THRESHOLD = 0.8; // 80% used -> grow
    static constexpr size_t DEFAULT_GROWTH_SIZE = 512;

    SessionPoolBase() : _totalAllocated(0), _availableCount(0), _isGrowing(false)
    {
    }

    ~SessionPoolBase()
    {
        Clear();
    }

    void Clear()
    {
        T *session;
        while (_pool.try_dequeue(session))
        {
            delete session;
        }
        _totalAllocated.store(0);
        _availableCount.store(0);
    }

    // [Production] Initial Warm-up: 1.2x expected CCU
    void WarmUp(size_t expectedCCU)
    {
        // 1.2x Warm-up using integer math
        size_t initialSize = expectedCCU + (expectedCCU / 5);
        LOG_INFO("[SessionPool] Warming up with {} sessions (Expected CCU: {})...", initialSize, expectedCCU);
        Grow(initialSize);
    }

    T *Acquire()
    {
        T *session;
        if (_pool.try_dequeue(session))
        {
            size_t available = _availableCount.fetch_sub(1) - 1;
            size_t total = _totalAllocated.load();

            // Proactive Check: If available < 20% of total (80% used), grow in background
            if (available * 5 < total)
            {
                TriggerBackgroundGrowth();
            }

            return session;
        }

        // [Emergency Fix] If pool is totally empty, block once to grow
        LOG_WARN("[SessionPool] Pool exhausted! Emergency blocking growth initiated.");
        Grow(DEFAULT_GROWTH_SIZE);

        if (_pool.try_dequeue(session))
        {
            _availableCount.fetch_sub(1);
            return session;
        }

        return nullptr;
    }

    void Release(T *session)
    {
        if (session)
        {
            _pool.enqueue(session);
            _availableCount.fetch_add(1);
        }
    }

private:
    void TriggerBackgroundGrowth()
    {
        // Atomically set _isGrowing to true only if it was false
        bool expected = false;
        if (!_isGrowing.compare_exchange_strong(expected, true))
            return; // Growth already in progress

        // Standard Practice: Growth in background to avoid blocking logic thread
        // Use detached thread for fire-and-forget background maintenance
        std::thread(
            [this]()
            {
                LOG_INFO(
                    "[SessionPool] Background growing... (Current: {}, Added: {})",
                    _totalAllocated.load(),
                    DEFAULT_GROWTH_SIZE
                );
                Grow(DEFAULT_GROWTH_SIZE);
                _isGrowing.store(false);
            }
        ).detach();
    }

    void Grow(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            _pool.enqueue(new T());
        }
        _totalAllocated.fetch_add(count);
        _availableCount.fetch_add(count);
    }

private:
    moodycamel::ConcurrentQueue<T *> _pool;
    std::atomic<size_t> _totalAllocated;
    std::atomic<size_t> _availableCount;
    std::atomic<bool> _isGrowing;
};

template <typename T> SessionPoolBase<T> &GetSessionPool()
{
    static SessionPoolBase<T> instance;
    return instance;
}

using SessionPoolGateway = SessionPoolBase<GatewaySession>;
using SessionPoolBackend = SessionPoolBase<BackendSession>;
using SessionPoolUDP = SessionPoolBase<UDPSession>;

} // namespace System
