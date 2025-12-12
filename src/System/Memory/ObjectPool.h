#pragma once

#include "System/Pch.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>

namespace System {

template <typename T> class ObjectPool {
public:
    static std::shared_ptr<T> Acquire() {
        T *ptr = nullptr;
        if (_pool.try_dequeue(ptr)) {
            // Reuse existing
            return std::shared_ptr<T>(ptr, [](T *p) { Release(p); });
        }

        // Create new
        ptr = new T();
        return std::shared_ptr<T>(ptr, [](T *p) { Release(p); });
    }

    // Instance-based pooling (Manual management)
    T *Pop() {
        T *ptr = nullptr;
        if (_instancePool.try_dequeue(ptr)) {
            return ptr;
        }
        return new T();
    }

    void Push(T *ptr) { _instancePool.enqueue(ptr); }

private:
    static void Release(T *ptr) { _pool.enqueue(ptr); }

    static moodycamel::ConcurrentQueue<T *> _pool;
    moodycamel::ConcurrentQueue<T *> _instancePool;
};

template <typename T> moodycamel::ConcurrentQueue<T *> ObjectPool<T>::_pool;

} // namespace System
