#pragma once
#include <cstddef>
#include <cstdint>


namespace System {

// [PacketView]
// A read-only view of a packet.
// This decouples the application from the internal memory layout (PacketMessage).
// Pass by Value (Cost is 2 pointers + 1 integer).
class PacketView
{
public:
    PacketView(uint16_t id, const uint8_t *payload, size_t length) : _id(id), _payload(payload), _length(length)
    {
    }

    uint16_t GetId() const
    {
        return _id;
    }
    size_t GetLength() const
    {
        return _length;
    }
    const uint8_t *GetPayload() const
    {
        return _payload;
    }

    // Helper: Parse Protobuf directly from the payload
    template <typename ProtoMsg> bool Parse(ProtoMsg &outMsg) const
    {
        return outMsg.ParseFromArray(_payload, static_cast<int>(_length));
    }

private:
    uint16_t _id;
    const uint8_t *_payload;
    size_t _length;
};

} // namespace System
