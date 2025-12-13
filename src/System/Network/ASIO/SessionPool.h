#pragma once

#include "System/Pch.h"
#include <cassert>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>
#include <thread>

namespace System {

/*
    [Zero-Contention SessionPool for Massive Scale MMORPG]

    Key Strategies:
    1. Pre-Allocation: Allocate all sessions upfront (no runtime atomic increment)
    2. Lock-Free Queue: moodycamel already provides per-thread tokens (no CAS on fast path)
    3. Soft Limit: Use watermark monitoring instead of hard blocking
    4. NUMA-Aware: Can be extended with per-node pools
*/

template <typename T>
concept Recyclable = requires(T t) {
    { t.Reset() } -> std::same_as<void>;
};

template <typename T> class SessionPool
{
public:
    // [Design Choice] Pre-allocate entire pool at startup
    // For 500k sessions: ~500k * sizeof(Session) memory upfront
    // Trade-off: Memory vs Zero Runtime Contention
    static void Init(size_t maxSessions, size_t preAllocate = 0)
    {
        assert(maxSessions > 0 && "maxSessions must be > 0");

        if (preAllocate == 0)
        {
            preAllocate = maxSessions; // Default: Full pre-allocation
        }

        assert(preAllocate <= maxSessions);

        _maxSessions = maxSessions;

        // Pre-allocate sessions (one-time cost at startup)
        for (size_t i = 0; i < preAllocate; ++i)
        {
            T *session = new T();
            _pool.enqueue(session);
        }

        _preAllocated = preAllocate;
        _totalCreated.store(preAllocate, std::memory_order_relaxed);

        // For monitoring/logging
        // LOG_INFO("SessionPool initialized: max={}, preallocated={}", maxSessions, preAllocate);
    }

    // [Hot Path] Zero-contention acquire with ProducerToken
    static T *Acquire()
    {
        T *ptr = nullptr;

        // Fast path: Lock-free dequeue with implicit per-thread token
        if (_pool.try_dequeue(ptr))
        {
            // [Lifecycle] Reset state for reuse
            if constexpr (Recyclable<T>)
            {
                ptr->Reset();
            }
            return ptr;
        }

        // Slow path: Pool exhausted
        // Option 1: Lazy allocation (if not fully pre-allocated)
        size_t created = _totalCreated.load(std::memory_order_relaxed);
        if (created < _maxSessions)
        {
            // Try allocate (rare contention here is acceptable)
            size_t expected = created;
            while (expected < _maxSessions)
            {
                if (_totalCreated.compare_exchange_weak(
                        expected, expected + 1, std::memory_order_release, std::memory_order_relaxed
                    ))
                {
                    ptr = new T();
                    if constexpr (Recyclable<T>)
                    {
                        ptr->Reset();
                    }
                    return ptr;
                }
            }
        }

        // Option 2: Hard cap reached - connection rejection
        // Increment rejection counter (for monitoring)
        _rejectionCount.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    // [Hot Path] Zero-contention release with ProducerToken
    static void Release(T *ptr)
    {
        if (!ptr)
            return;

        // Optional pre-pool cleanup hook
        if constexpr (requires { ptr->OnRecycle(); })
        {
            ptr->OnRecycle();
        }

        // Lock-free enqueue (no soft cap check for max throughput)
        // Note: Pool can temporarily exceed soft cap, but that's fine
        _pool.enqueue(ptr);
    }

    // [Advanced] Per-Thread Token for even better performance
    // Use this for long-lived worker threads
    struct ThreadToken
    {
        moodycamel::ProducerToken prodToken;
        moodycamel::ConsumerToken consToken;

        ThreadToken() : prodToken(_pool), consToken(_pool)
        {
        }
    };

    static T *AcquireWithToken(ThreadToken &token)
    {
        T *ptr = nullptr;
        if (_pool.try_dequeue(token.consToken, ptr))
        {
            if constexpr (Recyclable<T>)
            {
                ptr->Reset();
            }
            return ptr;
        }

        // Fallback to regular acquire
        return Acquire();
    }

    static void ReleaseWithToken(ThreadToken &token, T *ptr)
    {
        if (!ptr)
            return;

        if constexpr (requires { ptr->OnRecycle(); })
        {
            ptr->OnRecycle();
        }

        _pool.enqueue(token.prodToken, ptr);
    }

    // Graceful shutdown
    static void Shutdown()
    {
        T *ptr = nullptr;
        size_t destroyed = 0;

        while (_pool.try_dequeue(ptr))
        {
            delete ptr;
            ++destroyed;
        }

        size_t leaked = _totalCreated.load() - destroyed;
        if (leaked > 0)
        {
            // LOG_WARN("SessionPool leaked {} sessions", leaked);
        }

        _totalCreated.store(0, std::memory_order_relaxed);
    }

    // Non-intrusive monitoring (for ops dashboard)
    static size_t GetApproximatePoolSize()
    {
        return _pool.size_approx();
    }

    static size_t GetTotalCreated()
    {
        return _totalCreated.load(std::memory_order_relaxed);
    }

    static size_t GetApproximateActiveCount()
    {
        return _totalCreated.load(std::memory_order_relaxed) - _pool.size_approx();
    }

    static size_t GetRejectionCount()
    {
        return _rejectionCount.load(std::memory_order_relaxed);
    }

    static void ResetRejectionCount()
    {
        _rejectionCount.store(0, std::memory_order_relaxed);
    }

private:
    static moodycamel::ConcurrentQueue<T *> _pool;

    // Cold data (rarely accessed in hot path)
    inline static size_t _maxSessions = 0;
    inline static size_t _preAllocated = 0;

    // Relaxed atomics for monitoring (no hot path contention)
    static std::atomic<size_t> _totalCreated;
    static std::atomic<size_t> _rejectionCount;
};

// Static member initialization
template <typename T> moodycamel::ConcurrentQueue<T *> SessionPool<T>::_pool;
template <typename T> std::atomic<size_t> SessionPool<T>::_totalCreated{0};
template <typename T> std::atomic<size_t> SessionPool<T>::_rejectionCount{0};

} // namespace System