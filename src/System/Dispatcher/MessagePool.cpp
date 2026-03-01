#include "System/Dispatcher/MessagePool.h"
#include "System/Dispatcher/SystemMessages.h"
#include "System/Metrics/IMetrics.h"
#include "System/Pch.h"
#include <new> // for placement new
#include <vector>

namespace System {

std::atomic<int> MessagePool::_poolSize = 0;
moodycamel::ConcurrentQueue<void *> *MessagePool::_smallPool = new moodycamel::ConcurrentQueue<void *>();
moodycamel::ConcurrentQueue<void *> *MessagePool::_mediumPool = new moodycamel::ConcurrentQueue<void *>();
moodycamel::ConcurrentQueue<void *> *MessagePool::_largePool = new moodycamel::ConcurrentQueue<void *>();

int MessagePool::GetPoolSize()
{
    return _poolSize.load();
}

struct L1Cache
{
    std::vector<void *> smallBuffer;
    std::vector<void *> mediumBuffer;
    std::vector<void *> largeBuffer;

    L1Cache()
    {
        smallBuffer.reserve(MessagePool::L1_CACHE_SIZE);
        mediumBuffer.reserve(MessagePool::L1_CACHE_SIZE);
        largeBuffer.reserve(MessagePool::L1_CACHE_SIZE);
    }
};

static thread_local L1Cache t_l1;

PacketMessage *MessagePool::AllocatePacket(uint16_t bodySize)
{
    // [Metrics] Record sizing distribution
    if (bodySize <= 64)
        GetMetrics().GetCounter("msgpool_alloc_64b")->Increment();
    else if (bodySize <= 128)
        GetMetrics().GetCounter("msgpool_alloc_128b")->Increment();
    else if (bodySize <= 256)
        GetMetrics().GetCounter("msgpool_alloc_256b")->Increment();
    else if (bodySize <= 512)
        GetMetrics().GetCounter("msgpool_alloc_512b")->Increment();
    else if (bodySize <= 1024)
        GetMetrics().GetCounter("msgpool_alloc_1kb")->Increment();
    else if (bodySize <= 2048)
        GetMetrics().GetCounter("msgpool_alloc_2kb")->Increment();
    else if (bodySize <= 4096)
        GetMetrics().GetCounter("msgpool_alloc_4kb")->Increment();
    else if (bodySize <= 8192)
        GetMetrics().GetCounter("msgpool_alloc_8kb")->Increment();
    else if (bodySize <= 16384)
        GetMetrics().GetCounter("msgpool_alloc_16kb")->Increment();
    else
        GetMetrics().GetCounter("msgpool_alloc_over16kb")->Increment();

    size_t sizeLevel = 0; // 0: Small, 1: Medium, 2: Large
    if (bodySize <= SMALL_BODY_SIZE)
        sizeLevel = 0;
    else if (bodySize <= MEDIUM_BODY_SIZE)
        sizeLevel = 1;
    else if (bodySize <= LARGE_BODY_SIZE)
        sizeLevel = 2;
    else
    {
        // OS Heap Fallback for extremely large packets
        GetMetrics().GetCounter("msgpool_alloc_heap")->Increment();
        GetMetrics().GetCounter("msgpool_heap_bytes")->Increment(bodySize);

        size_t allocSize = PacketMessage::CalculateAllocSize(bodySize);
        void *block = ::operator new(allocSize);
        PacketMessage *msg = new (block) PacketMessage();
        msg->type = MessageType::PACKET;
        msg->length = bodySize;
        msg->isPooled = false;
        return msg;
    }

    void *block = PopBlock(sizeLevel);
    if (!block)
        return nullptr;

    PacketMessage *msg = new (block) PacketMessage();
    msg->type = MessageType::PACKET;
    msg->length = bodySize;
    msg->isPooled = true;

    return msg;
}

EventMessage *MessagePool::AllocateEvent()
{
    // Event messages are typically small, so use the small pool (sizeLevel 0)
    void *block = PopBlock(0);
    if (!block)
        return nullptr;
    EventMessage *msg = new (block) EventMessage();
    return msg;
}

TimerExpiredMessage *MessagePool::AllocateTimerExpired()
{
    // Timer messages are typically small, so use the small pool (sizeLevel 0)
    void *block = PopBlock(0);
    if (!block)
        return nullptr;
    TimerExpiredMessage *msg = new (block) TimerExpiredMessage();
    msg->type = MessageType::LOGIC_TIMER_EXPIRED;
    return msg;
}

TimerAddMessage *MessagePool::AllocateTimerAdd()
{
    // Timer messages are typically small, so use the small pool (sizeLevel 0)
    void *block = PopBlock(0);
    if (!block)
        return nullptr;
    TimerAddMessage *msg = new (block) TimerAddMessage();
    msg->type = MessageType::LOGIC_TIMER_ADD;
    return msg;
}

TimerCancelMessage *MessagePool::AllocateTimerCancel()
{
    // Timer messages are typically small, so use the small pool (sizeLevel 0)
    void *block = PopBlock(0);
    if (!block)
        return nullptr;
    TimerCancelMessage *msg = new (block) TimerCancelMessage();
    msg->type = MessageType::LOGIC_TIMER_CANCEL;
    return msg;
}

TimerTickMessage *MessagePool::AllocateTimerTick()
{
    // Timer messages are typically small, so use the small pool (sizeLevel 0)
    void *block = PopBlock(0);
    if (!block)
        return nullptr;
    TimerTickMessage *msg = new (block) TimerTickMessage();
    msg->type = MessageType::LOGIC_TIMER_TICK;
    return msg;
}

void MessagePool::Free(IMessage *msg)
{
    if (!msg)
        return;

    if (msg->DecRef())
    {
        if (!msg->isPooled)
        {
            delete msg;
            return;
        }

        size_t sizeLevel = 0; // Default to Small (Timers, Events)
        if (msg->type == MessageType::PACKET)
        {
            auto pkt = static_cast<PacketMessage *>(msg);
            if (pkt->length <= SMALL_BODY_SIZE)
                sizeLevel = 0;
            else if (pkt->length <= MEDIUM_BODY_SIZE)
                sizeLevel = 1;
            else
                sizeLevel = 2;
        }

        msg->~IMessage();
        PushBlock(static_cast<void *>(msg), sizeLevel);
    }
}

void MessagePool::FreeRaw(void *block)
{
    if (block)
    {
        // Default to Medium for raw blocks (historical compatibility)
        PushBlock(block, 1);
    }
}

void *MessagePool::PopBlock(size_t sizeLevel)
{
    std::vector<void *> *cache = nullptr;
    moodycamel::ConcurrentQueue<void *> *targetPool = nullptr;
    size_t blockSize = 0;

    if (sizeLevel == 0)
    {
        cache = &t_l1.smallBuffer;
        targetPool = _smallPool;
        blockSize = BLOCK_SIZE_SMALL;
    }
    else if (sizeLevel == 1)
    {
        cache = &t_l1.mediumBuffer;
        targetPool = _mediumPool;
        blockSize = BLOCK_SIZE_MEDIUM;
    }
    else
    {
        cache = &t_l1.largeBuffer;
        targetPool = _largePool;
        blockSize = BLOCK_SIZE_LARGE;
    }

    if (!cache->empty())
    {
        void *ptr = cache->back();
        cache->pop_back();
        return ptr;
    }

    void *bulkBuffer[BULK_TRANSFER_COUNT];
    size_t count = targetPool->try_dequeue_bulk(bulkBuffer, BULK_TRANSFER_COUNT);

    if (count > 0)
    {
        _poolSize.fetch_sub((int)count, std::memory_order_relaxed);
        if (count > 1)
        {
            cache->insert(cache->end(), bulkBuffer + 1, bulkBuffer + count);
        }
        return bulkBuffer[0];
    }

    return ::operator new(blockSize);
}

void MessagePool::PushBlock(void *block, size_t sizeLevel)
{
    std::vector<void *> *cache = nullptr;
    moodycamel::ConcurrentQueue<void *> *targetPool = nullptr;

    if (sizeLevel == 0)
    {
        cache = &t_l1.smallBuffer;
        targetPool = _smallPool;
    }
    else if (sizeLevel == 1)
    {
        cache = &t_l1.mediumBuffer;
        targetPool = _mediumPool;
    }
    else
    {
        cache = &t_l1.largeBuffer;
        targetPool = _largePool;
    }

    if (cache->size() < L1_CACHE_SIZE)
    {
        cache->push_back(block);
    }
    else
    {
        void *bulkBuffer[BULK_TRANSFER_COUNT];
        for (size_t i = 0; i < BULK_TRANSFER_COUNT; ++i)
        {
            bulkBuffer[i] = cache->back();
            cache->pop_back();
        }

        targetPool->enqueue_bulk(bulkBuffer, BULK_TRANSFER_COUNT);
        _poolSize.fetch_add((int)BULK_TRANSFER_COUNT, std::memory_order_relaxed);

        cache->push_back(block);
    }
}

void MessagePool::Prepare(size_t smallCount, size_t mediumCount, size_t largeCount)
{
    auto prepareLevel = [](moodycamel::ConcurrentQueue<void *> *pool, size_t count, size_t blockSize)
    {
        void *bulkBuffer[BULK_TRANSFER_COUNT];
        size_t current = 0;
        for (size_t i = 0; i < count; ++i)
        {
            bulkBuffer[current++] = ::operator new(blockSize);
            if (current == BULK_TRANSFER_COUNT)
            {
                pool->enqueue_bulk(bulkBuffer, current);
                _poolSize.fetch_add((int)current, std::memory_order_relaxed);
                current = 0;
            }
        }
        if (current > 0)
        {
            pool->enqueue_bulk(bulkBuffer, current);
            _poolSize.fetch_add((int)current, std::memory_order_relaxed);
        }
    };

    prepareLevel(_smallPool, smallCount, BLOCK_SIZE_SMALL);
    prepareLevel(_mediumPool, mediumCount, BLOCK_SIZE_MEDIUM);
    prepareLevel(_largePool, largeCount, BLOCK_SIZE_LARGE);
}

void MessagePool::Clear()
{
    auto clearPool = [](moodycamel::ConcurrentQueue<void *> *pool)
    {
        void *ptr = nullptr;
        while (pool->try_dequeue(ptr))
        {
            ::operator delete(ptr);
        }
    };

    clearPool(_smallPool);
    clearPool(_mediumPool);
    clearPool(_largePool);
}

} // namespace System
