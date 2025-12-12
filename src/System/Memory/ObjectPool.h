#pragma once

#include "System/Pch.h"
#include <atomic> // 추가
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <cstdio> // printf용

namespace System {

template <typename T> class ObjectPool
{
    // [검증용] 파괴 여부 플래그
    std::atomic<bool> _debugDestructed = false;

public:
    static std::atomic<int> _poolSize; // Track pool size
    static int GetPoolSize()
    {
        return _poolSize.load();
    }

    static std::shared_ptr<T> Acquire()
    {
        T *ptr = nullptr;
        if (_pool.try_dequeue(ptr))
        {
            _poolSize--;
            // printf("[Pool] Reused object. Pool Size: %d\n", _poolSize.load());
            return std::shared_ptr<T>(
                ptr,
                [](T *p)
                {
                    Release(p);
                }
            );
        }

        ptr = new T();
        // printf("[Pool] Alloc new object.\n");
        return std::shared_ptr<T>(
            ptr,
            [](T *p)
            {
                Release(p);
            }
        );
    }

    ~ObjectPool()
    {
        _debugDestructed = true;

        // [로그] 나 죽는다!
        printf("\n>> [DEBUG] ObjectPool Destructor Called! (Addr: %p)\n", this);

        T *ptr = nullptr;
        while (_instancePool.try_dequeue(ptr))
        {
            delete ptr;
        }
    }

    // Instance-based pooling
    T *Pop()
    {
        // [검증] 죽었으면 new 해서 리턴 (크래시 방지 + 로그)
        if (_debugDestructed)
        {
            printf(">> [CRITICAL] ObjectPool Pop called AFTER destruction! (Addr: %p)\n", this);
            return new T();
        }

        T *ptr = nullptr;
        if (_instancePool.try_dequeue(ptr))
        {
            return ptr;
        }
        return new T();
    }

    void Push(T *ptr)
    {
        // [검증] 죽었으면 그냥 delete (크래시 방지 + 로그)
        if (_debugDestructed)
        {
            printf(">> [CRITICAL] ObjectPool Push called AFTER destruction! (Addr: %p)\n", this);
            delete ptr;
            return;
        }
        _instancePool.enqueue(ptr);
    }

    static void Clear()
    {
        T *ptr = nullptr;
        while (_pool.try_dequeue(ptr))
        {
            delete ptr;
        }
    }

private:
    static void Release(T *ptr)
    {
        // 정적 풀은 일단 놔둡니다 (이번 타겟은 인스턴스 풀)
        _pool.enqueue(ptr);
        _poolSize++;
        // printf("[Pool] Released object. Pool Size: %d\n", _poolSize.load());
    }

    static moodycamel::ConcurrentQueue<T *> _pool;
    moodycamel::ConcurrentQueue<T *> _instancePool;
};

template <typename T> moodycamel::ConcurrentQueue<T *> ObjectPool<T>::_pool;
template <typename T> std::atomic<int> ObjectPool<T>::_poolSize = 0;

} // namespace System