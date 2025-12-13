#include "System/Memory/PacketPool.h"

namespace System {

// Static Member Initialization
std::atomic<int> PacketPool::_poolSize{0};
moodycamel::ConcurrentQueue<Packet *> *PacketPool::_pool = new moodycamel::ConcurrentQueue<Packet *>();
thread_local std::vector<Packet *> PacketPool::_l1Cache; // TLS Definition

// Implementation of Packet Release Callback
// Called by Packet::intrusive_ptr_release
void Packet_Release_To_Pool(Packet *p)
{
    // Return to pool (Zero Alloc)
    PacketPool::Push(p);
}

} // namespace System