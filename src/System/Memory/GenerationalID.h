#pragma once

#include <cstdint>
#include <functional>

namespace System {

/*
    [GenerationalID]
    A 32-bit handle serving as a safe weak reference to pooled objects.
    - Top 16 bits: Version (Increments each time the slot is reused)
    - Bottom 16 bits: Index (The slot or raw ID in the array/pool)

    Resolves the ABA problem at the logic level. If an entity dies and
    another spawns in the same slot, the Version will mismatch, safely
    preventing dangling pointer interactions.
*/
struct GenerationalID
{
    uint32_t value = 0;

    constexpr GenerationalID() noexcept = default;
    constexpr explicit GenerationalID(uint32_t val) noexcept : value(val)
    {
    }
    constexpr GenerationalID(uint16_t version, uint16_t index) noexcept
        : value((static_cast<uint32_t>(version) << 16) | static_cast<uint32_t>(index))
    {
    }

    constexpr uint16_t GetVersion() const noexcept
    {
        return static_cast<uint16_t>(value >> 16);
    }
    constexpr uint16_t GetIndex() const noexcept
    {
        return static_cast<uint16_t>(value & 0xFFFF);
    }

    constexpr bool IsValid() const noexcept
    {
        return value != 0;
    }

    constexpr bool operator==(const GenerationalID &rhs) const noexcept
    {
        return value == rhs.value;
    }
    constexpr bool operator!=(const GenerationalID &rhs) const noexcept
    {
        return value != rhs.value;
    }

    // Allow implicit conversion to int32_t for legacy map keys if needed
    constexpr operator int32_t() const noexcept
    {
        return static_cast<int32_t>(value);
    }
};

} // namespace System

namespace std {
template <> struct hash<System::GenerationalID>
{
    size_t operator()(const System::GenerationalID &id) const noexcept
    {
        return std::hash<uint32_t>{}(id.value);
    }
};
} // namespace std
