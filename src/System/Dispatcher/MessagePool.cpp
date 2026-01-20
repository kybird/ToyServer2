#include "System/Dispatcher/MessagePool.h"
#include "System/Dispatcher/SystemMessages.h"
#include "System/Pch.h"
#include <new> // for placement new
#include <vector>

namespace System {

std::atomic<int> MessagePool::_poolSize = 0;
moodycamel::ConcurrentQueue<void *> *MessagePool::_pool = new moodycamel::ConcurrentQueue<void *>();

int MessagePool::GetPoolSize()
{
    return _poolSize.load();
}

struct L1Cache
{
    std::vector<void *> buffer;
    L1Cache()
    {
        buffer.reserve(MessagePool::L1_CACHE_SIZE);
    }
};

static thread_local L1Cache t_l1;

PacketMessage *MessagePool::AllocatePacket(uint16_t bodySize)
{
    // [Constraint] If bodySize > MAX, we must fail or fallback to malloc.
    // For now, assume Hot Path packets are always <= 4KB.
    if (bodySize > MAX_PACKET_BODY_SIZE)
    {
        return nullptr; // Error case
    }

    void *block = PopBlock();
    if (!block)
        return nullptr;

    // Placement New
    PacketMessage *msg = new (block) PacketMessage();
    msg->type = (uint32_t)MessageType::NETWORK_DATA;
    msg->length = bodySize;
    // Data is ready to be written to msg->data

    return msg;
}

EventMessage *MessagePool::AllocateEvent()
{
    void *block = PopBlock();
    if (!block)
        return nullptr;
    EventMessage *msg = new (block) EventMessage();
    msg->type = (uint32_t)MessageType::NETWORK_CONNECT; // Default or ignored?
    // EventMessage typically doesn't use type for dispatch logic in same way or has specific types?
    // IMessage.h has NETWORK_CONNECT, NETWORK_DISCONNECT, etc.
    // AllocateEvent is generic.
    return msg;
}

TimerMessage *MessagePool::AllocateTimer()
{
    void *block = PopBlock();
    if (!block)
        return nullptr;
    TimerMessage *msg = new (block) TimerMessage();
    msg->type = static_cast<uint32_t>(MessageType::LOGIC_TIMER);
    // Re-using LOGIC_JOB or adding LOGIC_TIMER in future?
    // Let's check IMessage.h for MessageType enum. It has LOGIC_JOB.
    // Let's stick to LOGIC_JOB or add LOGIC_TIMER?
    // I should add LOGIC_TIMER to enum in IMessage.h FIRST if I want strict typing.
    // Previous step I didn't add it. I should fix IMessage.h enum too.
    return msg;
}

TimerExpiredMessage *MessagePool::AllocateTimerExpired()
{
    void *block = PopBlock();
    if (!block)
        return nullptr;
    TimerExpiredMessage *msg = new (block) TimerExpiredMessage();
    msg->type = static_cast<uint32_t>(MessageType::LOGIC_TIMER_EXPIRED);
    return msg;
}

TimerAddMessage *MessagePool::AllocateTimerAdd()
{
    void *block = PopBlock();
    if (!block)
        return nullptr;
    TimerAddMessage *msg = new (block) TimerAddMessage();
    msg->type = static_cast<uint32_t>(MessageType::LOGIC_TIMER_ADD);
    return msg;
}

TimerCancelMessage *MessagePool::AllocateTimerCancel()
{
    void *block = PopBlock();
    if (!block)
        return nullptr;
    TimerCancelMessage *msg = new (block) TimerCancelMessage();
    msg->type = static_cast<uint32_t>(MessageType::LOGIC_TIMER_CANCEL);
    return msg;
}

TimerTickMessage *MessagePool::AllocateTimerTick()
{
    void *block = PopBlock();
    if (!block)
        return nullptr;
    TimerTickMessage *msg = new (block) TimerTickMessage();
    msg->type = static_cast<uint32_t>(MessageType::LOGIC_TIMER_TICK);
    return msg;
}

void MessagePool::Free(IMessage *msg)
{
    if (!msg)
        return;

    // [RefCount] Decrement and only free if 0
    if (msg->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        // Destructor? trivial for these structs, but good practice if limits change.
        msg->~IMessage();

        // Check if PacketMessage and use larger size?
        // MessagePool uses FIXED_BLOCK_SIZE for everything currently.
        PushBlock(static_cast<void *>(msg));
    }
}

void MessagePool::FreeRaw(void *block)
{
    // [Deprecated] Removed
    if (block)
    {
        PushBlock(block);
    }
}

void *MessagePool::PopBlock()
{
    void *ptr = nullptr;

    if (!t_l1.buffer.empty())
    {
        ptr = t_l1.buffer.back();
        t_l1.buffer.pop_back();
    }
    else
    {
        void *bulkBuffer[BULK_TRANSFER_COUNT];
        size_t count = _pool->try_dequeue_bulk(bulkBuffer, BULK_TRANSFER_COUNT);

        if (count > 0)
        {
            _poolSize.fetch_sub((int)count, std::memory_order_relaxed);
            ptr = bulkBuffer[0];
            if (count > 1)
            {
                // Fill cache
                t_l1.buffer.insert(t_l1.buffer.end(), bulkBuffer + 1, bulkBuffer + count);
            }
        }
        else
        {
            // Allocate Raw Memory Block
            ptr = ::operator new(FIXED_BLOCK_SIZE);
        }
    }
    return ptr;
}

void MessagePool::PushBlock(void *block)
{
    if (t_l1.buffer.size() < L1_CACHE_SIZE)
    {
        t_l1.buffer.push_back(block);
    }
    else
    {
        size_t transferCount = BULK_TRANSFER_COUNT;
        void *bulkBuffer[BULK_TRANSFER_COUNT];

        for (size_t i = 0; i < transferCount; ++i)
        {
            bulkBuffer[i] = t_l1.buffer.back();
            t_l1.buffer.pop_back();
        }

        _pool->enqueue_bulk(bulkBuffer, transferCount);
        _poolSize.fetch_add((int)transferCount, std::memory_order_relaxed);

        t_l1.buffer.push_back(block);
    }
}

void MessagePool::Prepare(size_t count)
{
    void *bulkBuffer[BULK_TRANSFER_COUNT];
    size_t current = 0;

    for (size_t i = 0; i < count; ++i)
    {
        bulkBuffer[current++] = ::operator new(FIXED_BLOCK_SIZE);

        if (current == BULK_TRANSFER_COUNT)
        {
            _pool->enqueue_bulk(bulkBuffer, current);
            _poolSize.fetch_add((int)current, std::memory_order_relaxed);
            current = 0;
        }
    }
    if (current > 0)
    {
        _pool->enqueue_bulk(bulkBuffer, current);
        _poolSize.fetch_add((int)current, std::memory_order_relaxed);
    }
}

void MessagePool::Clear()
{
    void *ptr = nullptr;
    while (_pool->try_dequeue(ptr))
    {
        ::operator delete(ptr);
    }
}

} // namespace System
