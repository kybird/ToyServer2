#pragma once
#include "Share/Protocol.h"
#include "System/Packet/PacketBase.h"
#include <cstring>
#include <span>

namespace Share {

class EchoPacket : public System::PacketBase<EchoPacket, PacketHeader>
{
    std::span<const uint8_t> _payload;

public:
    static constexpr uint16_t ID = PacketType::PKT_S_ECHO;

    EchoPacket() = default;
    EchoPacket(std::span<const uint8_t> payload) : _payload(payload)
    {
    }

    size_t GetBodySize() const
    {
        return _payload.size();
    }

    void SerializeBodyTo(void *buffer) const
    {
        if (!_payload.empty())
        {
            std::memcpy(buffer, _payload.data(), _payload.size());
        }
    }
};

} // namespace Share
