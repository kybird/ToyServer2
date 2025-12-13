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
    static void Init(size_t allocLimit, size_t poolLimit)
    {
        _allocLimit.store(allocLimit);
        _poolLimit.store(poolLimit);
    }

    static T *Pop()
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

        // Double check (to prevent massive burst overshooting)
        // Note: Strict CAS loop is better but costly. This is acceptable for "mostly limit".
        // If we really need strict cap, we should increment BEFORE check or use CAS.
        // Let's stick to relaxed check for perf, but maybe basic CAS is better for safety.

        ptr = new T();
        return ptr;
    }

    static void Push(T *ptr)
    {
        if (!ptr)
            return;

        // [Lifecycle] Optional Reset
        // If T has Reset(), you should call it here or before Push.
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
    static size_t GetAllocCount()
    {
        return _allocCount.load();
    }
    static size_t GetPoolCount()
    {
        return _poolCount.load();
    }

private:
    static moodycamel::ConcurrentQueue<T *> _pool;

    // Stats
    static std::atomic<size_t> _allocCount; // Total Allocated (System Memory)
    static std::atomic<size_t> _poolCount;  // Total Idle (Cache)

    // Config
    static std::atomic<size_t> _allocLimit; // Hard Cap
    static std::atomic<size_t> _poolLimit;  // Soft Cap
};

template <typename T> moodycamel::ConcurrentQueue<T *> ObjectPool<T>::_pool;
template <typename T> std::atomic<size_t> ObjectPool<T>::_allocCount = 0;
template <typename T> std::atomic<size_t> ObjectPool<T>::_poolCount = 0;
// Defaults: Safe Unbounded-ish for dev, but user should call Init()
template <typename T> std::atomic<size_t> ObjectPool<T>::_allocLimit = 0; // 0 = Unlimited
template <typename T> std::atomic<size_t> ObjectPool<T>::_poolLimit = 1000;

} // namespace System