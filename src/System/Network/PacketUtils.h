#pragma once

#include <cstdint>

namespace System {

struct PacketMessage;

class PacketUtils
{
public:
    // Allocation
    // The returned packet is owned by the Caller until it is passed to Session::Send()
    static PacketMessage *CreatePacket(uint16_t size);

    static PacketMessage *CreatePacket(const PacketMessage *src);

    // Deallocation
    // Use this if the packet was created but NOT sent (e.g. error condition)
    static void ReleasePacket(PacketMessage *pkt);
};

} // namespace System
