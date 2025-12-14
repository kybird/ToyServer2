#pragma once

#include <cstdint>

namespace System {

class AsioSession; // Direct type, no ISession abstraction in message

enum class MessageType { NETWORK_CONNECT, NETWORK_DISCONNECT, NETWORK_DATA, LOGIC_JOB };

// [New Architecture: Single Allocation Polymorphism]
struct IMessage
{
    virtual ~IMessage() = default;
    MessageType type;
    uint64_t sessionId;
    AsioSession *session = nullptr; // Direct pointer, no vtable
};

struct PacketMessage : public IMessage
{
    // uint16_t packetId; // Can be parsed from data if needed, or stored here.
    // For now, raw data containment.
    uint16_t length;
    uint8_t data[1]; // Flexible Array Member

    uint8_t *Payload()
    {
        return data;
    }
    const uint8_t *Payload() const
    {
        return data;
    }

    // Helper to get total allocation size needed
    static size_t CalculateAllocSize(uint16_t bodySize)
    {
        // sizeof(PacketMessage) includes the first byte of data[1].
        // So we need sizeof(PacketMessage) - 1 + bodySize.
        // But for safety and alignment, usually sizeof(PacketMessage) + bodySize is fine (1 byte waste).
        return sizeof(PacketMessage) + bodySize;
    }
};

struct TimerMessage : public IMessage
{
    uint32_t timerId;
    uint32_t contextId;
    // Add other timer data as needed
};

// Generic System Event (Connect/Disconnect)
struct EventMessage : public IMessage
{
    // No extra data needed, just Header info
};

} // namespace System
