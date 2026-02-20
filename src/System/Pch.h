#pragma once

// Standard Library
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Third-party
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <fmt/format.h>
#include <format>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

// Common Defines will go here

// Cross-platform 128-bit unsigned integer struct
struct uint128_t
{
    uint64_t high;
    uint64_t low;

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

// UINT128_MAX equivalent
constexpr uint128_t UINT128_MAX = uint128_t(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL);

// std::hash specialization for uint128_t
namespace System {
struct uint128_hash
{
    size_t operator()(const uint128_t &key) const noexcept
    {
        std::hash<uint64_t> hasher;
        return hasher(key.high) ^ (hasher(key.low) + 0x9e3779b97f4a7c15);
    }
};
} // namespace System
