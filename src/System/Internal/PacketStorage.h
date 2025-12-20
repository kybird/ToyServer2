#pragma once
#include <atomic>
#include <cstdint>

namespace System {

// [Internal Only]
// Immutable, broadcast-safe packet storage
struct PacketStorage
{
    std::atomic<uint32_t> refCount{1};
    uint16_t size;
    uint16_t packetId;
    uint8_t payload[1]; // flexible array (or at least start of payload)

    void AddRef()
    {
        refCount.fetch_add(1, std::memory_order_relaxed);
    }

    void Release();
};

} // namespace System
