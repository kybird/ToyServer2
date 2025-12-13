#include "System/Memory/PacketPool.h"

namespace System {

// Static Member Initialization
std::atomic<int> PacketPool::_poolSize{0};
moodycamel::ConcurrentQueue<Packet *> *PacketPool::_pool = new moodycamel::ConcurrentQueue<Packet *>();
thread_local std::vector<Packet *> PacketPool::_l1Cache; // TLS Definition

// Implementation of intrusive_ptr_release
// Moved here to break circular dependency: Packet.h -> PacketPool.h -> Packet.h
void intrusive_ptr_release(Packet *p)
{
    // relaxed order is sufficient, we just want to know when it hits 0
    if (p->_refCount.fetch_sub(1, std::memory_order_release) == 1)
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        PacketPool::Push(p);
    }
}

} // namespace System