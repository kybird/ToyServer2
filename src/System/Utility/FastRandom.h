#pragma once
#include <cstdint>

namespace System {
namespace Utility {

/**
 * @brief Fast random number generator using Xorshift128+
 * @note Much faster than std::mt19937 for game logic
 */
class FastRandom
{
public:
    FastRandom() : FastRandom(1234567890ULL)
    {
    }

    explicit FastRandom(uint64_t seed)
    {
        _state[0] = seed;
        _state[1] = seed ^ 0x123456789ABCDEFULL;

        // Warm up
        for (int i = 0; i < 10; ++i)
        {
            Next();
        }
    }

    /**
     * @brief Generate next random number [0, UINT64_MAX]
     */
    uint64_t Next()
    {
        uint64_t s1 = _state[0];
        const uint64_t s0 = _state[1];
        _state[0] = s0;
        s1 ^= s1 << 23;
        _state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
        return _state[1] + s0;
    }

    /**
     * @brief Generate random float [0.0, 1.0)
     */
    float NextFloat()
    {
        return static_cast<float>(Next() >> 40) / 16777216.0f;
    }

    /**
     * @brief Generate random float [min, max)
     */
    float NextFloat(float min, float max)
    {
        return min + NextFloat() * (max - min);
    }

    /**
     * @brief Generate random int [min, max]
     */
    int32_t NextInt(int32_t min, int32_t max)
    {
        return min + static_cast<int32_t>(Next() % (max - min + 1));
    }

    /**
     * @brief Generate random bool with given probability
     * @param probability Probability of returning true [0.0, 1.0]
     */
    bool NextBool(float probability = 0.5f)
    {
        return NextFloat() < probability;
    }

private:
    uint64_t _state[2];
};

} // namespace Utility
} // namespace System
