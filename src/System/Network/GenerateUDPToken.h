#pragma once

#include <cstdint>
#include <random>

namespace System {

/**
 * @brief UDP 패킷용 토큰 생성 유틸리티
 */

class GenerateUDPToken
{
public:
    /**
     * @brief 128비트 랜덤 토큰 생성
     * @return 생성된 토큰 값
     */
    static uint128_t Generate()
    {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 rng(rd());
        static std::uniform_int_distribution<uint128_t> dist(0, UINT128_MAX);

        return static_cast<uint128_t>(dist(rng));
    }
};

} // namespace System
