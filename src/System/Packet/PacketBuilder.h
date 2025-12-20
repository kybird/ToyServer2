#pragma once
#include "System/Dispatcher/MessagePool.h"
#include "System/Packet/IPacket.h"
#include <new>

namespace System {

// [Internal]
// IPacket -> PacketStorage
struct PacketBuilder
{
    static PacketMessage *Build(const IPacket &pkt)
    {
        uint16_t size = pkt.GetTotalSize();

        auto msg = MessagePool::AllocatePacket(size);
        if (!msg)
            return nullptr;

        // MessagePool init sets refCount=1
        // msg->packetId = pkt.GetPacketId(); // PacketMessage doesn't store ID separate from data usually?
        // PacketHeader is inside data.

        pkt.SerializeTo(msg->Payload());

        return msg;
    }
};

} // namespace System
