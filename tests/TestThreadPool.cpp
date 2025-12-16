#include "System/Thread/ThreadPool.h"
#include <atomic>
#include <gtest/gtest.h>
#include <vector>

class ThreadPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Teardown code if needed
    }
};

// Test Simple Task Execution
TEST_F(ThreadPoolTest, SimpleTask)
{
    System::ThreadPool pool(2);
    pool.Start();

    auto future = pool.Enqueue(
        []()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return 42;
        }
    );

    EXPECT_EQ(future.get(), 42);

    pool.Stop();
}

// Test Concurrency
TEST_F(ThreadPoolTest, Concurrency)
{
    System::ThreadPool pool(4);
    pool.Start();

    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 1000; ++i)
    {
        futures.push_back(pool.Enqueue(
            [&counter]()
            {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        ));
    }

    for (auto &f : futures)
    {
        f.get();
    }

    EXPECT_EQ(counter.load(), 1000);

    pool.Stop();
}

// Test Exception Propagation
TEST_F(ThreadPoolTest, ExceptionPropagation)
{
    System::ThreadPool pool(2);
    pool.Start();

    auto future = pool.Enqueue(
        []() -> int
        {
            throw std::runtime_error("Test exception");
            return 0;
        }
    );

    EXPECT_THROW(future.get(), std::runtime_error);

    pool.Stop();
}

// Test Graceful Shutdown
TEST_F(ThreadPoolTest, GracefulShutdown)
{
    System::ThreadPool pool(2);
    pool.Start();

    std::atomic<int> completed{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 10; ++i)
    {
        futures.push_back(pool.Enqueue(
            [&completed]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                completed.fetch_add(1);
            }
        ));
    }

    pool.Stop(true); // finishPending = true

    // Check if all tasks completed
    EXPECT_EQ(completed.load(), 10);
}

// Performance Benchmark (Informational)
TEST_F(ThreadPoolTest, Performance)
{
    System::ThreadPool pool(8);
    pool.Start();

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10000; ++i)
    {
        futures.push_back(pool.Enqueue(
            [i]()
            {
                return i * i;
            }
        ));
    }

    for (auto &f : futures)
    {
        f.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "[INFO] 10000 tasks completed in " << duration.count() << "ms\n";

    // Simple sanity check performance threshold (e.g., < 2000ms)
    EXPECT_LT(duration.count(), 2000);

    pool.Stop();
}
