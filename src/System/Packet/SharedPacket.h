#pragma once
#include "System/Internal/PacketStorage.h"
#include "System/Packet/IPacket.h"
#include <cstring>


namespace System {

// Read-only view
class SharedPacket final : public IPacket
{
public:
    explicit SharedPacket(PacketStorage *storage) : _storage(storage)
    {
    }

    uint16_t GetPacketId() const override
    {
        return _storage->packetId;
    }

    uint16_t GetTotalSize() const override
    {
        return _storage->size;
    }

    void SerializeTo(void *buffer) const override
    {
        std::memcpy(buffer, _storage->payload, _storage->size);
    }

    const uint8_t *Data() const
    {
        return _storage->payload;
    }

    PacketStorage *Storage() const
    {
        return _storage;
    }

private:
    PacketStorage *_storage;
};

} // namespace System
