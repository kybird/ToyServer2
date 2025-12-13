#pragma once

#include "System/Network/Packet.h"
#include "System/Pch.h"
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <concurrentqueue/moodycamel/concurrentqueue.h>

namespace System {

class PacketPool
{
public:
    static std::atomic<int> _poolSize; // Track GLOBAL pool size (L2)
    static int GetPoolSize()
    {
        return _poolSize.load();
    }

    // Tuning Constants
    static const size_t L1_CACHE_SIZE = 1000;
    static const size_t BULK_TRANSFER_COUNT = 500;

    static boost::intrusive_ptr<Packet> Allocate(size_t size)
    {
        Packet *ptr = nullptr;

        // 1. Try L1 Cache (Thread Local, Lock-Free)
        if (!_l1Cache.empty())
        {
            ptr = _l1Cache.back();
            _l1Cache.pop_back();
        }
        else
        {
            // 2. L1 is empty -> Replenish from L2 (Global, Locked)
            // Prepare buffer for bulk dequeue
            Packet *bulkBuffer[BULK_TRANSFER_COUNT];
            size_t count = _pool->try_dequeue_bulk(bulkBuffer, BULK_TRANSFER_COUNT);

            if (count > 0)
            {
                _poolSize.fetch_sub((int)count, std::memory_order_relaxed);

                // Keep one for return
                ptr = bulkBuffer[0];

                // Store rest in L1
                if (count > 1)
                {
                    _l1Cache.insert(_l1Cache.end(), bulkBuffer + 1, bulkBuffer + count);
                }
            }
            else
            {
                // 3. L2 is also empty -> Last Resort: Allocate New
                ptr = new Packet(size);
                // Note: We don't increment poolSize here, it tracks cached availability
            }
        }

        ptr->Reset(); // Reset internal state and refcount
        if (ptr->capacity() < size)
            ptr->reserve(size);

        // intrusive_ptr constructor calls add_ref.
        // Packet::Reset set refCount to 0, so this makes it 1.
        return boost::intrusive_ptr<Packet>(ptr);
    }

    static void Push(Packet *ptr)
    {
        // 1. Try L1 Cache
        if (_l1Cache.size() < L1_CACHE_SIZE)
        {
            _l1Cache.push_back(ptr);
        }
        else
        {
            // 2. L1 is full -> Offload Lock to L2 (Global)
            // Move some packets from L1 to L2 to make space
            // Strategy: Keep half, move half (BULK_TRANSFER_COUNT)

            // We need to move 'BULK_TRANSFER_COUNT' items from L1 to L2.
            // But 'ptr' is the one triggering this.

            // Simpler strategy: Just push this one to L2? No, contention.
            // Better: Move cached items to bulk array, enqueue bulk.

            // Take items from back of L1
            size_t transferCount = BULK_TRANSFER_COUNT;

            // Ensure we have enough (we check size < L1_CACHE_SIZE, but here size >= L1_CACHE_SIZE)
            // L1_CACHE_SIZE (1000) > BULK (500)

            Packet *bulkBuffer[BULK_TRANSFER_COUNT + 1]; // +1 for current ptr if needed, but we free from cache

            // Copy from L1
            for (size_t i = 0; i < transferCount; ++i)
            {
                bulkBuffer[i] = _l1Cache.back();
                _l1Cache.pop_back();
            }

            // Enqueue bulk
            _pool->enqueue_bulk(bulkBuffer, transferCount);
            _poolSize.fetch_add((int)transferCount, std::memory_order_relaxed);

            // Now L1 has space for 'ptr'
            _l1Cache.push_back(ptr);
        }
    }

    static void Prepare(size_t count, size_t defaultSize = 4096)
    {
        // Prepare fills Global L2 directly
        Packet *bulkBuffer[BULK_TRANSFER_COUNT];
        size_t current = 0;

        for (size_t i = 0; i < count; ++i)
        {
            bulkBuffer[current++] = new Packet(defaultSize);

            if (current == BULK_TRANSFER_COUNT)
            {
                _pool->enqueue_bulk(bulkBuffer, current);
                _poolSize.fetch_add((int)current, std::memory_order_relaxed);
                current = 0;
            }
        }
        // flush remainder
        if (current > 0)
        {
            _pool->enqueue_bulk(bulkBuffer, current);
            _poolSize.fetch_add((int)current, std::memory_order_relaxed);
        }
    }

    static void Clear()
    {
        // Clear L2
        Packet *ptr = nullptr;
        while (_pool->try_dequeue(ptr))
        {
            delete ptr;
        }
        // Note: Can't easily clear other threads' L1 caches.
        // They will leak at shutdown unless we implement thread-exit handlers.
        // For a server, this is usually acceptable at shutdown OS cleanup.
    }

private:
    // Queue of raw pointers (L2 Global Pool)
    static moodycamel::ConcurrentQueue<Packet *> *_pool;

    // L1 Thread Local Cache
    static thread_local std::vector<Packet *> _l1Cache;
};

} // namespace System