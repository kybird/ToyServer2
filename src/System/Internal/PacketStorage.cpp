#include "System/Internal/PacketStorage.h"
#include "System/Dispatcher/IMessage.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Pch.h"

namespace System {

void PacketStorage::Release()
{
    if (refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        // [Constraint] Reusing MessagePool
        // PacketStorage is allocated via MessagePool (reusing PacketMessage block),
        // so we must free it via MessagePool.
        // [Fix] Use FreeRaw to avoid invoking ~IMessage() because vptr is overwritten by refCount!
        MessagePool::FreeRaw(this);
    }
}

} // namespace System
