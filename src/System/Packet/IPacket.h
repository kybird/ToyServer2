#pragma once
#include <cstdint>

namespace System {

// [IPacket] 사용자에게 노출되는 패킷 인터페이스
// PacketBase CRTP는 이 인터페이스를 구현함
class IPacket
{
public:
    virtual ~IPacket() = default;
    virtual uint16_t GetPacketId() const = 0;
    virtual uint16_t GetTotalSize() const = 0;
    virtual void SerializeTo(void *buffer) const = 0;
};

} // namespace System
