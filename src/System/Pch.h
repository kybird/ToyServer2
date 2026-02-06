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

// Cross-platform 128-bit unsigned integer definition
#if defined(_MSC_VER)
    // MSVC supports unsigned __int128
    using uint128_t = unsigned __int128;
#else
    // GCC/Clang support __uint128_t
    using uint128_t = __uint128_t;
#endif

// UINT128_MAX is not defined by standard, so we define it
constexpr uint128_t UINT128_MAX = ~static_cast<uint128_t>(0);

// std::hash specialization for uint128_t (needed for MSVC unsigned __int128)
namespace System
{
    struct uint128_hash
    {
        size_t operator()(const uint128_t& key) const noexcept
        {
            #if defined(_MSC_VER)
                // MSVC: hash based on high and low 64-bit parts
                uint64_t high = static_cast<uint64_t>(key >> 64);
                uint64_t low = static_cast<uint64_t>(key);
                std::hash<uint64_t> hasher;
                return hasher(high) ^ (hasher(low) + 0x9e3779b97f4a7c15);
            #else
                // GCC/Clang: simple hash based on both parts
                uint64_t high = static_cast<uint64_t>(key >> 64);
                uint64_t low = static_cast<uint64_t>(key);
                std::hash<uint64_t> hasher;
                return hasher(high) ^ hasher(low);
            #endif
        }
    };
}
