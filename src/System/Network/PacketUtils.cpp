#include "System/Network/PacketUtils.h"
#include "System/Dispatcher/IMessage.h"
#include "System/Dispatcher/MessagePool.h"


namespace System {

PacketMessage *PacketUtils::CreatePacket(uint16_t size)
{
    return MessagePool::AllocatePacket(size);
}

PacketMessage *PacketUtils::CreatePacket(const PacketMessage *src)
{
    if (!src)
        return nullptr;
    auto *pkt = MessagePool::AllocatePacket(src->length);
    if (pkt)
    {
        std::memcpy(pkt->Payload(), src->Payload(), src->length);
    }
    return pkt;
}

void PacketUtils::ReleasePacket(PacketMessage *pkt)
{
    if (pkt)
    {
        MessagePool::Free(pkt);
    }
}

} // namespace System
