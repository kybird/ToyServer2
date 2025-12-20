#pragma once

#include <cstddef>
#include <cstdint>


namespace System {

#pragma pack(push, 1)
struct PacketHeader
{
    using SizeType = uint16_t;
    using IdType = uint16_t;

    static constexpr size_t SIZE = sizeof(SizeType) + sizeof(IdType);

    SizeType size; // Total size including header
    IdType id;     // PacketType
};
#pragma pack(pop)

} // namespace System
