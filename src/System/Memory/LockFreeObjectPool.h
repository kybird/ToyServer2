#pragma once

#include "System/Memory/RefCounted.h"
#include "System/Memory/RefPtr.h"
#include "System/Pch.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <type_traits>

namespace System {

/*
    [LockFreeObjectPool]
    A thread-safe, ABA-problem-defended object pool built on moodycamel::ConcurrentQueue.
    Designed specifically to yield RefPtr<T> to guarantee atomic transitions
    where the ref count increments safely to 1 upon leaving the pool.

    LockFreeObjectPool requirements:
    1. T must have an implementation of Reset() method to reset inner state upon retrieval.
    2. T will be automatically ref-counted at `Pop()`, starting with a 1 Count.
    3. Pop() returns RefPtr<T>.
    4. Type Constraint: T must inherit from RefCounted and implement Reset().
*/

// [Type Constraint] Ensure T is poolable and safely ref-counted.
// We use duck-typing to verify RefCounted inheritance because
// MSVC often fails template base deduction for indirect CRTP bases.
template <typename T>
concept PoolableEntity = requires(T t) {
    { t.AddRef() };
    { t.Release() };
    { t.GetRefCount() };
    { t.Reset() };
};

template <PoolableEntity T> class LockFreeObjectPool
{
public:
    LockFreeObjectPool() = default;
    ~LockFreeObjectPool() = default;

    // [Init] Call this once at startup
    // allocLimit: Max total objects alive (In-Use + Pooled). 0 = Unlimited (Dangerous)
    // poolLimit: Max idle objects in pool.
    void Init(size_t allocLimit, size_t poolLimit)
    {
        _allocLimit.store(allocLimit, std::memory_order_relaxed);
        _poolLimit.store(poolLimit, std::memory_order_relaxed);
        // _pool is a value member, already constructed. No need to assign make_unique.
    }

    RefPtr<T> Pop()
    {
        T *ptr = nullptr;

        // 1. Try Reuse (Fast Path from moodycamel queue)
        if (_pool.try_dequeue(ptr))
        {
            _poolCount.fetch_sub(1, std::memory_order_relaxed);

            // [Strict Constraint] Enforce Reset on recycled objects
            if constexpr (requires { ptr->Reset(); })
            {
                ptr->Reset();
            }

            // [Strict Constraint] Return RefPtr<T>(ptr) so refCount atomically becomes 1
            return System::RefPtr<T>(ptr);
        }

        // 2. Try Allocate (Scanning Hard Cap)
        size_t currentAlloc = _allocCount.load(std::memory_order_relaxed);
        size_t limit = _allocLimit.load(std::memory_order_relaxed);

        if (limit > 0 && currentAlloc >= limit)
        {
            // [Back-Pressure] Hard Cap Reached.
            return nullptr;
        }

        // Optimistic Allocation
        _allocCount.fetch_add(1, std::memory_order_relaxed);
        ptr = new T();

        // The constructor of T happens. We don't need to call Reset() on fresh allocation,
        // but it's safe to just return it wrapped in RefPtr.
        // Also note that RefPtr(T*) increments refCount from 0 to 1 automatically.
        return System::RefPtr<T>(ptr);
    }

    // [Push] Return an object to the pool.
    void Push(T *obj)
    {
        if (!obj)
            return;

        // In this architecture, Push is typically called by RefCounted::ReturnToPool
        // when refCount drops to 0.
        // We do NOT call Reset() here. It's called in Pop() to keep the object warm
        // in cache but pristine before being handed out.

        size_t currentPool = _poolCount.load(std::memory_order_relaxed);
        size_t limit = _poolLimit.load(std::memory_order_relaxed);

        if (currentPool < limit)
        {
            _pool.enqueue(obj);
            _poolCount.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            // Overflow -> OS Return
            delete obj;
            _allocCount.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    // Diagnostics
    size_t GetAllocCount() const
    {
        return _allocCount.load(std::memory_order_relaxed);
    }
    size_t GetPoolCount() const
    {
        return _poolCount.load(std::memory_order_relaxed);
    }

private:
    moodycamel::ConcurrentQueue<T *> _pool;

    // Stats
    std::atomic<size_t> _allocCount{0}; // Total Allocated (System Memory)
    std::atomic<size_t> _poolCount{0};  // Total Idle (Cache)

    // Config
    std::atomic<size_t> _allocLimit{0};    // Hard Cap. 0 = Unlimited
    std::atomic<size_t> _poolLimit{10000}; // Soft Cap
};

} // namespace System
