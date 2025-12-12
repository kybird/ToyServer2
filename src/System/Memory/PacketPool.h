#pragma once

#include "System/Pch.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <vector>

namespace System {

class PacketPool
{
public:
    static std::atomic<int> _poolSize; // Track pool size
    static int GetPoolSize()
    {
        return _poolSize.load();
    }

    static std::shared_ptr<std::vector<uint8_t>> Allocate(size_t size)
    {
        std::vector<uint8_t> *ptr = nullptr;

        // [수정] 포인터 접근(->)으로 변경
        if (_pool->try_dequeue(ptr))
        {
            _poolSize--;
            ptr->clear();
            if (ptr->capacity() < size)
                ptr->reserve(size);
        }
        else
        {
            ptr = new std::vector<uint8_t>();
            ptr->reserve(size);
        }

        return std::shared_ptr<std::vector<uint8_t>>(
            ptr,
            [](std::vector<uint8_t> *p)
            {
                Release(p);
            }
        );
    }

    static void Clear()
    {
        std::vector<uint8_t> *ptr = nullptr;
        // [수정] 포인터 접근
        while (_pool->try_dequeue(ptr))
        {
            delete ptr;
        }
    }

private:
    static void Release(std::vector<uint8_t> *ptr)
    {
        // [핵심] _pool은 포인터라서 프로그램 끝까지 살아있음
        _pool->enqueue(ptr);
        _poolSize++;
    }

    // [수정] 객체(Queue)가 아니라 포인터(Queue*)로 선언
    static moodycamel::ConcurrentQueue<std::vector<uint8_t> *> *_pool;
};

} // namespace System