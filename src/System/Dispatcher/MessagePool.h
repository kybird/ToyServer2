#pragma once

#include "System/Dispatcher/IMessage.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <vector>

namespace System {

class MessagePool
{
public:
    static std::atomic<int> _poolSize;
    static int GetPoolSize()
    {
        return _poolSize.load();
    }

    // Constants
    static const size_t MAX_PACKET_BODY_SIZE = 4096; // 4KB Body limit for pooled messages
    static const size_t FIXED_BLOCK_SIZE = sizeof(PacketMessage) + MAX_PACKET_BODY_SIZE; // Max size we allocate
    static const size_t L1_CACHE_SIZE = 1000;
    static const size_t BULK_TRANSFER_COUNT = 500;

    // Allocation
    static PacketMessage *AllocatePacket(uint16_t bodySize);
    static EventMessage *AllocateEvent();
    static TimerMessage *AllocateTimer(); // TBD

    // Deallocation
    static void Free(IMessage *msg);

    // Management
    static void Prepare(size_t count);
    static void Clear();

private:
    static moodycamel::ConcurrentQueue<void *> *_pool; // Stores raw blocks of FIXED_BLOCK_SIZE

    // Allocates a raw block of FIXED_BLOCK_SIZE
    static void *PopBlock();
    static void PushBlock(void *block);
};

} // namespace System