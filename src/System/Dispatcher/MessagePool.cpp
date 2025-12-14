#include "System/Dispatcher/MessagePool.h"
#include "System/Pch.h"
#include <new> // for placement new
#include <vector>

namespace System {

std::atomic<int> MessagePool::_poolSize = 0;
moodycamel::ConcurrentQueue<void *> *MessagePool::_pool = new moodycamel::ConcurrentQueue<void *>();

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
    msg->type = MessageType::NETWORK_DATA;
    msg->length = bodySize;
    // Data is ready to be written to msg->data

    return msg;
}

EventMessage *MessagePool::AllocateEvent()
{
    void *block = PopBlock();
    EventMessage *msg = new (block) EventMessage();
    // type set by caller or defaults?
    // msg->type is not set in constructor.
    return msg;
}

void MessagePool::Free(IMessage *msg)
{
    if (!msg)
        return;

    // Destructor? trivial for these structs, but good practice if limits change.
    msg->~IMessage();

    PushBlock(static_cast<void *>(msg));
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
