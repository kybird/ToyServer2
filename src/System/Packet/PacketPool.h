#pragma once

#include "System/Memory/ObjectPool.h"
#include <new>     // for placement new
#include <utility> // for std::forward


namespace System {

template <typename TPacket> class PacketPool
{
private:
    static System::ObjectPool<TPacket> &GetPool()
    {
        static System::ObjectPool<TPacket> _pool;
        return _pool;
    }

public:
    // Allocate with arguments
    template <typename... Args> static TPacket *Allocate(Args &&...args)
    {
        TPacket *ptr = GetPool().Pop();
        if (!ptr)
        {
            // Fallback: If allocation fails (hard cap) or new allocation
            // ObjectPool::Pop() returns new T() if allowed.
            // If it returns nullptr (Hard Cap), we return nullptr.
            // But ObjectPool::Pop() only returns nullptr if Alloc Limit reached.
            // Assuming unlimited for now or handled by Pop logic.
            // Wrapper: If Pop returns nullptr, we can't allocate.
            return nullptr;
        }

        // If Pop returned a pointer, it points to a valid memory block (either new or reused).
        // We use placement new to construct efficiently.
        new (ptr) TPacket(std::forward<Args>(args)...);
        return ptr;
    }

    static void Release(TPacket *packet)
    {
        if (packet)
        {
            // Destruct properly before returning to pool
            packet->~TPacket();
            GetPool().Push(packet);
        }
    }
};
} // namespace System
