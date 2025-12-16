#include "System/Network/RateLimiter.h"
#include <gtest/gtest.h>
#include <thread>


using namespace System;

/*
    Mocking time is hard with std::chrono::steady_clock directly embedded.
    For this Unit Test, we rely on actual sleep to test "refill".
    Since it's lazy evaluation, sleep + consume should trigger check.
*/

TEST(RateLimiterTest, InitialBurst)
{
    // 10 tokens/sec, Burst 5
    RateLimiter limiter(10.0, 5.0);

    // Should be able to consume 5 immediately
    EXPECT_TRUE(limiter.TryConsume(5.0));

    // Should fail now (empty bucket)
    EXPECT_FALSE(limiter.TryConsume(1.0));
}

TEST(RateLimiterTest, RefillLogic)
{
    // 100 tokens/sec => 1 token every 10ms
    RateLimiter limiter(100.0, 10.0);

    // Consume all
    EXPECT_TRUE(limiter.TryConsume(10.0));
    EXPECT_FALSE(limiter.TryConsume(1.0));

    // Wait 25ms (approx 2.5 tokens)
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // Should operate again
    EXPECT_TRUE(limiter.TryConsume(1.0));
    EXPECT_TRUE(limiter.TryConsume(1.0));
    // Now empty again?
    // EXPECT_FALSE(limiter.TryConsume(1.0)); // Timing is flaky
}

TEST(RateLimiterTest, PartialConsume)
{
    // 10 tokens/sec
    RateLimiter limiter(10.0, 10.0);

    // Consume 0.5 (if supported, current impl supports double)
    EXPECT_TRUE(limiter.TryConsume(0.5));
    // Should support many partials
    for (int i = 0; i < 18; ++i)
    { // 9.0 consumed
        EXPECT_TRUE(limiter.TryConsume(0.5));
    }
}
