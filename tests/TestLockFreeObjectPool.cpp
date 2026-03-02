#include "System/Memory/LockFreeObjectPool.h"
#include "System/Memory/RefCounted.h"
#include "System/Memory/RefPtr.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace System {
namespace Testing {

class StressObject : public RefCounted<StressObject>
{
public:
    int id;
    int data[16]; // Padding to make the object a bit larger

    StressObject() : id(-1)
    {
        // Simulate heavy constructor
        std::fill(std::begin(data), std::end(data), 0xDD);
    }

    virtual ~StressObject()
    {
    }

    // Reset required by constraint
    void Reset()
    {
        id = -1;
        std::fill(std::begin(data), std::end(data), 0x00);
    }

    void ReturnToPool();
};

// Global pool instance to allow ReturnToPool to reach back and push it.
// Moved AFTER StressObject definition to satisfy PoolableEntity constraint.
static LockFreeObjectPool<StressObject> *g_StressPool = nullptr;

void StressObject::ReturnToPool()
{
    if (g_StressPool)
    {
        g_StressPool->Push(this);
    }
    else
    {
        delete this; // Fallback
    }
}

class LockFreeObjectPoolStressTest : public ::testing::Test
{
protected:
    LockFreeObjectPool<StressObject> pool;

    void SetUp() override
    {
        pool.Init(50000, 50000); // Allow lots of objects
        g_StressPool = &pool;
    }

    void TearDown() override
    {
        g_StressPool = nullptr;
    }
};

TEST_F(LockFreeObjectPoolStressTest, ValidPopStateAndReset)
{
    auto ptr = pool.Pop();
    ASSERT_NE(ptr.get(), nullptr);
    EXPECT_EQ(ptr->GetRefCount(), 1);

    // Test data state
    ptr->id = 999;

    // Release drops back down to 0, ReturnToPool puts it in queue
    ptr = nullptr;
    EXPECT_EQ(pool.GetPoolCount(), 1);
    EXPECT_EQ(pool.GetAllocCount(), 1);

    // Re-pop it
    auto ptr2 = pool.Pop();
    EXPECT_EQ(pool.GetPoolCount(), 0);
    EXPECT_EQ(ptr2->GetRefCount(), 1);

    // Ensure Reset was called inside Pop
    EXPECT_EQ(ptr2->id, -1);
    EXPECT_EQ(ptr2->data[0], 0);
}

TEST_F(LockFreeObjectPoolStressTest, MultiThreadedStressTest)
{
    const int numThreads = 16;
    const int opsPerThread = 5000; // Reduced for faster test execution

    std::atomic<int> runningThreads{0};
    std::atomic<bool> startFlag{false};

    auto stressWorker = [&]()
    {
        runningThreads++;
        while (!startFlag)
        {
            std::this_thread::yield();
        }

        for (int i = 0; i < opsPerThread; ++i)
        {
            auto p1 = pool.Pop();
            auto p2 = pool.Pop();
            auto p3 = pool.Pop();

            p1->id = i;
            p2->id = i + 1;
            p3->id = i + 2;

            // Random delay mix up
            if (i % 3 == 0)
            {
                p2 = nullptr;
            }

            if (i % 5 == 0)
            {
                p1 = nullptr;
                auto p4 = pool.Pop(); // Re-grab
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back(stressWorker);
    }

    while (runningThreads < numThreads)
    {
        std::this_thread::yield();
    }

    // Unleash all threads concurrently
    startFlag = true;

    for (auto &t : threads)
    {
        t.join();
    }

    // At this point, ALL thread-local RefPtrs went out of scope.
    // ALL objects should have cleanly returned to the pool (refCount == 0).
    size_t allocTotal = pool.GetAllocCount();
    size_t idleTotal = pool.GetPoolCount();

    EXPECT_EQ(allocTotal, idleTotal) << "There is a leak! " << allocTotal << " allocated, but only " << idleTotal
                                     << " returned to pool.";
}

} // namespace Testing
} // namespace System
