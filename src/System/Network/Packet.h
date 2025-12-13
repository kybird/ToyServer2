#pragma once
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <cstdint>
#include <cstring> // for memcpy


namespace System {

// [Thread Confinement Rule]
// Packet allows Non-Atomic RefCount because it is strictly confined to the Dispatcher Thread.
// Violation of this rule will cause Race Conditions.
struct Packet
{
    uint32_t refCount = 0; // Non-Atomic
    uint32_t size = 0;     // Current used size
    uint32_t capacity = 0; // Buffer capacity
    uint8_t *buffer = nullptr;

    void Reset()
    {
        size = 0;
        refCount = 0;
        // Capacity and buffer are preserved
    }

    uint8_t *data()
    {
        return buffer;
    }
    const uint8_t *data() const
    {
        return buffer;
    }

    // Helper to mimic vector interface for compatibility
    void assign(const uint8_t *start, const uint8_t *end)
    {
        size_t newSize = end - start;
        if (newSize > capacity)
        {
            // In a strict pool system, we can't easily reallocate in hot path.
            // For safety, truncate or error. Here we truncate to avoid crash.
            newSize = capacity;
        }
        size = static_cast<uint32_t>(newSize);
        if (newSize > 0)
            std::memcpy(buffer, start, newSize);
    }
};

// Forward declaration for Release (Implemented in PacketPool.cpp)
void Packet_Release_To_Pool(Packet *p);

inline void intrusive_ptr_add_ref(Packet *p)
{
    ++p->refCount;
}

inline void intrusive_ptr_release(Packet *p)
{
    if (--p->refCount == 0)
    {
        Packet_Release_To_Pool(p);
    }
}

} // namespace System
