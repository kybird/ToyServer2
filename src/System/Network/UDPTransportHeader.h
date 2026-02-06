#pragma once

#include <cstdint>

namespace System {

#pragma pack(push, 1)
struct UDPTransportHeader
{
    uint8_t tag;
    uint64_t sessionId;
    uint128_t udpToken;

    static constexpr size_t SIZE = sizeof(tag) + sizeof(sessionId) + sizeof(udpToken);
    static constexpr uint8_t TAG_RAW_UDP = 0x00;
    static constexpr uint8_t TAG_KCP = 0x01;

    bool IsValid() const
    {
        return tag == TAG_RAW_UDP || tag == TAG_KCP;
    }

    bool IsKCP() const
    {
        return tag == TAG_KCP;
    }

    bool IsRawUDP() const
    {
        return tag == TAG_RAW_UDP;
    }
};
#pragma pack(pop)

} // namespace System
