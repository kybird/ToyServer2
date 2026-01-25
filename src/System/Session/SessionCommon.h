#pragma once

#include "System/Network/Packet.h"
#include <cstdint>

namespace System {

/*
    Common utilities shared by GatewaySession and BackendSession.

    [Design]
    - No inheritance, just plain helper functions
    - Stateless, header-only for inlining
    - Focused on packet header validation and buffer manipulation
*/
namespace SessionCommon {

// Validate packet header size
inline bool IsValidPacketSize(uint16_t size)
{
    constexpr uint16_t MAX_PACKET_SIZE = 1024 * 10; // 10KB
    return size >= sizeof(PacketHeader) && size <= MAX_PACKET_SIZE;
}

// Check if buffer has enough data for a complete packet
inline bool HasCompletePacket(int32_t dataSize, uint16_t packetSize)
{
    return dataSize >= static_cast<int32_t>(packetSize);
}

// Extract packet header from buffer
inline PacketHeader *GetPacketHeader(uint8_t *buffer)
{
    return reinterpret_cast<PacketHeader *>(buffer);
}

} // namespace SessionCommon

} // namespace System
