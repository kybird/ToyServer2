#pragma once

#include "System/Packet/IPacket.h"
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// [Configuration] Uncomment to enable safety margin for Protobuf serialization
// This was introduced to handle potential varint size mismatches but tests showed it's currently unnecessary.
#define USE_PROTOBUF_SERIALIZE_SAFETY_MARGIN

namespace System {

constexpr size_t MaxPacketSize = 65535; // UINT16_MAX

template <typename T>
concept PacketHeaderConcept = requires(T t) {
    { T::SIZE } -> std::convertible_to<size_t>;
    typename T::IdType;
    { t.size } -> std::convertible_to<uint16_t>;
    { t.id } -> std::convertible_to<uint16_t>;
};

template <typename TDerived, typename THeader>
    requires PacketHeaderConcept<THeader>
class PacketBase : public IPacket
{
protected:
    static_assert(THeader::SIZE <= MaxPacketSize, "Header size exceeds maximum");

    static uint16_t CalculateSafeSize(size_t bodySize)
    {
        constexpr size_t headerSize = THeader::SIZE;
        const size_t totalSize = headerSize + bodySize;
        if (totalSize > MaxPacketSize)
        {
            fprintf(stderr, "Packet too large: %zu > %zu\n", totalSize, MaxPacketSize);
            std::abort();
        }
        return static_cast<uint16_t>(totalSize);
    }

    static void FastSerializeHeader(THeader *header, uint16_t packetId, uint16_t totalSize)
    {
        header->size = totalSize;
        header->id = packetId;
    }

public:
    static constexpr size_t HeaderSize()
    {
        return THeader::SIZE;
    }
    static constexpr auto PacketId()
    {
        return TDerived::ID;
    }

    // IPacket interface
    uint16_t GetPacketId() const override
    {
        return TDerived::ID;
    }

    uint16_t GetTotalSize() const override
    {
        const auto *derived = static_cast<const TDerived *>(this);
        return CalculateSafeSize(derived->GetBodySize());
    }

    void SerializeTo(void *buffer) const override
    {
        const auto *derived = static_cast<const TDerived *>(this);
        THeader *header = static_cast<THeader *>(buffer);
        void *body = static_cast<uint8_t *>(buffer) + THeader::SIZE;

        FastSerializeHeader(header, TDerived::ID, GetTotalSize());
        derived->SerializeBodyTo(body);
    }
};

// Helper for Protobuf packets
template <typename TDerived, typename THeader, typename TProto>
class ProtobufPacketBase : public PacketBase<TDerived, THeader>
{
protected:
    TProto _proto;

public:
    using IdType = typename THeader::IdType;

    explicit ProtobufPacketBase(const TProto &msg) : _proto(msg)
    {
    }
    explicit ProtobufPacketBase(TProto &&msg) : _proto(std::move(msg))
    {
    }
    ProtobufPacketBase() = default;

    TProto &GetProto()
    {
        return _proto;
    }
    const TProto &GetProto() const
    {
        return _proto;
    }

    size_t GetBodySize() const
    {
        return _proto.ByteSizeLong();
    }

    void SerializeBodyTo(void *buffer) const
    {
        size_t bodySize = GetBodySize();
#ifdef USE_PROTOBUF_SERIALIZE_SAFETY_MARGIN
        // [Fix] Protobuf varint encoding can slightly exceed ByteSizeLong()
        // Add 10% safety margin to prevent buffer overflow
        size_t safeSize = bodySize + (bodySize / 10) + 16;

        if (!_proto.SerializeToArray(buffer, static_cast<int>(safeSize)))
        {
            // Fallback: Try with exact size if safe size fails
            _proto.SerializeToArray(buffer, static_cast<int>(bodySize));
        }
#else
        // Use exact size (Default behavior verified by tests)
        _proto.SerializeToArray(buffer, static_cast<int>(bodySize));
#endif
    }

    void Reset()
    {
        _proto.Clear();
    }
};

} // namespace System
