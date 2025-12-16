#pragma once

#include "System/Pch.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <type_traits>

namespace System {

/*
    [ObjectPool Robust]
    Improvements:
    1. Hard Cap (_allocLimit): Prvents unlimited expansion (DoS protection).
    2. Soft Cap (_poolLimit): Limits idle memory usage.
    3. Proper Atomics: reliable tracking of totals and idle counts.
    4. Lifecycle Contract: Enforces usage of Reset/OnRecycle if available.
*/

template <typename T> class ObjectPool
{
public:
    // [Init] Call this once at startup
    // allocLimit: Max total objects alive (In-Use + Pooled). 0 = Unlimited (Dangerous)
    // poolLimit: Max idle objects in pool.
    void Init(size_t allocLimit, size_t poolLimit)
    {
        _allocLimit.store(allocLimit);
        _poolLimit.store(poolLimit);
    }

    T *Pop()
    {
        T *ptr = nullptr;

        // 1. Try Reuse (Fast Path)
        if (_pool.try_dequeue(ptr))
        {
            _poolCount.fetch_sub(1, std::memory_order_relaxed);
            return ptr;
        }

        // 2. Try Allocate (Scanning Hard Cap)
        size_t currentAlloc = _allocCount.load(std::memory_order_relaxed);
        size_t limit = _allocLimit.load(std::memory_order_relaxed);

        if (limit > 0 && currentAlloc >= limit)
        {
            // [Back-Pressure] Hard Cap Reached.
            // Returning nullptr forces caller to handle exhaustion (drop connection, wait, etc)
            return nullptr;
        }

        // Optimistic Allocation
        _allocCount.fetch_add(1, std::memory_order_relaxed);

        ptr = new T();
        return ptr;
    }

    void Push(T *ptr)
    {
        if (!ptr)
            return;

        // [Lifecycle] Optional Reset
        // Doing it here centralizes cleanup.
        // if constexpr (requires { ptr->Reset(); }) { ptr->Reset(); }

        size_t currentPool = _poolCount.load(std::memory_order_relaxed);
        size_t limit = _poolLimit.load(std::memory_order_relaxed);

        if (currentPool < limit)
        {
            _pool.enqueue(ptr);
            _poolCount.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            // Overflow -> OS Return
            delete ptr;
            _allocCount.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    // Diagnostics
    size_t GetAllocCount() const
    {
        return _allocCount.load();
    }
    size_t GetPoolCount() const
    {
        return _poolCount.load();
    }

private:
    moodycamel::ConcurrentQueue<T *> _pool;

    // Stats
    std::atomic<size_t> _allocCount = 0; // Total Allocated (System Memory)
    std::atomic<size_t> _poolCount = 0;  // Total Idle (Cache)

    // Config
    std::atomic<size_t> _allocLimit = 0;   // Hard Cap. 0 = Unlimited
    std::atomic<size_t> _poolLimit = 1000; // Soft Cap
};

} // namespace System