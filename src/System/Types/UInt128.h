#pragma once

#include <cstdint>
#include <functional>

namespace System {

/**
 * @brief Cross-platform 128-bit unsigned integer struct
 *
 * ms-windows/MSVC 환경 등 네이티브 128비트 자료형(__int128) 연산이 매끄럽게 지원되지 않는 경우
 * 크로스플랫폼 호환성과 시프트(<< 64)로 인한 UB(C4293) 방지를 위해 자체 128비트 값 형태를 정의합니다.
 */
struct uint128_t
{
    uint64_t high; // 상위 64비트
    uint64_t low;  // 하위 64비트

    uint128_t() = default;
    constexpr uint128_t(uint64_t l) : high(0), low(l)
    {
    }
    constexpr uint128_t(uint64_t h, uint64_t l) : high(h), low(l)
    {
    }

    bool operator==(const uint128_t &other) const
    {
        return high == other.high && low == other.low;
    }
    bool operator!=(const uint128_t &other) const
    {
        return !(*this == other);
    }
};

// UINT128 최대값 대응
constexpr uint128_t UINT128_MAX = uint128_t(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL);

/**
 * @brief System::uint128_t 해싱 알고리즘
 */
struct uint128_hash
{
    size_t operator()(const uint128_t &key) const noexcept
    {
        std::hash<uint64_t> hasher;
        // 0x9e3779b97f4a7c15 (Golden Ratio) 상수를 더하여 충돌 확률 소거.
        return hasher(key.high) ^ (hasher(key.low) + 0x9e3779b97f4a7c15);
    }
};

} // namespace System
