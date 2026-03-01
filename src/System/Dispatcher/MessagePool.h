#pragma once

#include "System/Dispatcher/IMessage.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <vector>

namespace System {

struct TimerExpiredMessage;
struct TimerAddMessage;
struct TimerCancelMessage;
struct TimerTickMessage;
struct EventMessage;

class MessagePool
{
public:
    static std::atomic<int> _poolSize;
    static int GetPoolSize();

    // Pool Level Constants
    static const size_t SMALL_BODY_SIZE = 1024;  // 1KB
    static const size_t MEDIUM_BODY_SIZE = 4096; // 4KB (기존 최대)
    static const size_t LARGE_BODY_SIZE = 16384; // 16KB (KCP 재조립 등 대용량 대응)

    // Allocation Block Sizes (sizeof(PacketMessage) + BodySize)
    static const size_t BLOCK_SIZE_SMALL = sizeof(PacketMessage) + SMALL_BODY_SIZE;
    static const size_t BLOCK_SIZE_MEDIUM = sizeof(PacketMessage) + MEDIUM_BODY_SIZE;
    static const size_t BLOCK_SIZE_LARGE = sizeof(PacketMessage) + LARGE_BODY_SIZE;

    static const size_t L1_CACHE_SIZE = 1000;
    static const size_t BULK_TRANSFER_COUNT = 500;

    // Allocation
    static PacketMessage *AllocatePacket(uint16_t bodySize);
    static EventMessage *AllocateEvent();

    static TimerExpiredMessage *AllocateTimerExpired();
    static TimerAddMessage *AllocateTimerAdd();
    static TimerCancelMessage *AllocateTimerCancel();
    static TimerTickMessage *AllocateTimerTick();

    // Deallocation
    static void Free(IMessage *msg);
    static void FreeRaw(void *block);

    // Management
    static void Prepare(size_t smallCount, size_t mediumCount, size_t largeCount);
    static void Clear();

private:
    // Multi-Level Queues
    static moodycamel::ConcurrentQueue<void *> *_smallPool;  // 1KB
    static moodycamel::ConcurrentQueue<void *> *_mediumPool; // 4KB
    static moodycamel::ConcurrentQueue<void *> *_largePool;  // 16KB

    // Helper functions for specific levels
    static void *PopBlock(size_t sizeLevel);
    static void PushBlock(void *block, size_t sizeLevel);
};

} // namespace System