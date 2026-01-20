/**
 * @file TestSmartNotifyBenchmark.cpp
 * @brief Benchmark test for Smart Notify optimization
 *
 * Measures:
 * 1. notify_one() call count comparison (Smart vs Always)
 * 2. System call overhead under high load
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// ============================================================================
// Mock Dispatcher for testing (Smart Notify OFF)
// ============================================================================
class AlwaysNotifyDispatcher
{
public:
    void Post(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(task));
        }
        _notifyCount.fetch_add(1, std::memory_order_relaxed);
        _cv.notify_one(); // Always notify
    }

    bool Process()
    {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_queue.empty())
                return false;
            task = std::move(_queue.front());
            _queue.pop();
        }
        task();
        return true;
    }

    void Wait(int timeoutMs)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait_for(lock, std::chrono::milliseconds(timeoutMs));
    }

    size_t GetNotifyCount() const
    {
        return _notifyCount.load();
    }

    size_t GetQueueSize() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

private:
    std::queue<std::function<void()>> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<size_t> _notifyCount{0};
};

// ============================================================================
// Mock Dispatcher for testing (Smart Notify ON)
// ============================================================================
class SmartNotifyDispatcher
{
public:
    void Post(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(task));
        }

        // Smart Notify: Only notify when there are waiting threads
        if (_waitingCount.load(std::memory_order_relaxed) > 0)
        {
            _notifyCount.fetch_add(1, std::memory_order_relaxed);
            _cv.notify_one();
        }
        else
        {
            _skippedCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    bool Process()
    {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_queue.empty())
                return false;
            task = std::move(_queue.front());
            _queue.pop();
        }
        task();
        return true;
    }

    void Wait(int timeoutMs)
    {
        _waitingCount.fetch_add(1, std::memory_order_relaxed);
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait_for(lock, std::chrono::milliseconds(timeoutMs));
        }
        _waitingCount.fetch_sub(1, std::memory_order_relaxed);
    }

    size_t GetNotifyCount() const
    {
        return _notifyCount.load();
    }
    size_t GetSkippedCount() const
    {
        return _skippedCount.load();
    }

    size_t GetQueueSize() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

private:
    std::queue<std::function<void()>> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<int32_t> _waitingCount{0};
    std::atomic<size_t> _notifyCount{0};
    std::atomic<size_t> _skippedCount{0};
};

// ============================================================================
// Benchmark Tests
// ============================================================================

class SmartNotifyBenchmark : public ::testing::Test
{
protected:
    static constexpr int MESSAGE_COUNT = 100000;
    static constexpr int PRODUCER_COUNT = 4;
};

TEST_F(SmartNotifyBenchmark, CompareNotifyCallCount)
{
    std::cout << "\n========================================\n";
    std::cout << " Smart Notify Benchmark\n";
    std::cout << " Messages: " << MESSAGE_COUNT << ", Producers: " << PRODUCER_COUNT << "\n";
    std::cout << "========================================\n\n";

    // ========== Always Notify ==========
    {
        AlwaysNotifyDispatcher dispatcher;
        std::atomic<bool> running{true};
        std::atomic<int> processed{0};

        std::thread consumer(
            [&]()
            {
                while (running || dispatcher.GetQueueSize() > 0)
                {
                    if (dispatcher.Process())
                        processed.fetch_add(1);
                    else
                        dispatcher.Wait(1);
                }
            }
        );

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> producers;
        for (int p = 0; p < PRODUCER_COUNT; ++p)
        {
            producers.emplace_back(
                [&]()
                {
                    for (int i = 0; i < MESSAGE_COUNT / PRODUCER_COUNT; ++i)
                    {
                        dispatcher.Post(
                            []()
                            {
                            }
                        );
                    }
                }
            );
        }

        for (auto &t : producers)
            t.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        running = false;
        consumer.join();

        std::cout << "[Always Notify]\n";
        std::cout << "  notify_one() calls: " << dispatcher.GetNotifyCount() << "\n";
        std::cout << "  Time: " << duration << "ms\n";
        std::cout << "  Processed: " << processed.load() << "\n\n";
    }

    // ========== Smart Notify ==========
    {
        SmartNotifyDispatcher dispatcher;
        std::atomic<bool> running{true};
        std::atomic<int> processed{0};

        std::thread consumer(
            [&]()
            {
                while (running || dispatcher.GetQueueSize() > 0)
                {
                    if (dispatcher.Process())
                        processed.fetch_add(1);
                    else
                        dispatcher.Wait(1);
                }
            }
        );

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> producers;
        for (int p = 0; p < PRODUCER_COUNT; ++p)
        {
            producers.emplace_back(
                [&]()
                {
                    for (int i = 0; i < MESSAGE_COUNT / PRODUCER_COUNT; ++i)
                    {
                        dispatcher.Post(
                            []()
                            {
                            }
                        );
                    }
                }
            );
        }

        for (auto &t : producers)
            t.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        running = false;
        consumer.join();

        std::cout << "[Smart Notify]\n";
        std::cout << "  notify_one() calls: " << dispatcher.GetNotifyCount() << "\n";
        std::cout << "  Skipped notifies: " << dispatcher.GetSkippedCount() << "\n";
        std::cout << "  Time: " << duration << "ms\n";
        std::cout << "  Processed: " << processed.load() << "\n\n";

        // Verify: Smart Notify should have fewer calls than Always Notify
        // (More skips when Consumer is busy processing)
        EXPECT_GT(dispatcher.GetSkippedCount(), 0);
    }

    std::cout << "========================================\n";
    std::cout << " Result: Smart Notify skips unnecessary\n";
    std::cout << " notify_one() when Consumer is busy.\n";
    std::cout << "========================================\n\n";
}

// High load simulation: When Consumer can't keep up with Producer
TEST_F(SmartNotifyBenchmark, HighLoadScenario)
{
    std::cout << "\n========================================\n";
    std::cout << " High Load Scenario (Slow Consumer)\n";
    std::cout << "========================================\n";

    SmartNotifyDispatcher dispatcher;
    std::atomic<bool> running{true};

    // Slow Consumer simulation (10us delay)
    std::thread consumer(
        [&]()
        {
            while (running || dispatcher.GetQueueSize() > 0)
            {
                if (dispatcher.Process())
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                else
                {
                    dispatcher.Wait(10);
                }
            }
        }
    );

    const int burstSize = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    // Fast burst send
    for (int i = 0; i < burstSize; ++i)
    {
        dispatcher.Post(
            []()
            {
            }
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto postDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Wait for processing to complete
    while (dispatcher.GetQueueSize() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    running = false;
    consumer.join();

    double skipRatio =
        100.0 * dispatcher.GetSkippedCount() / (dispatcher.GetNotifyCount() + dispatcher.GetSkippedCount());

    std::cout << "  Total Posts: " << burstSize << "\n";
    std::cout << "  notify_one() calls: " << dispatcher.GetNotifyCount() << "\n";
    std::cout << "  Skipped notifies: " << dispatcher.GetSkippedCount() << "\n";
    std::cout << "  Skip Ratio: " << std::fixed << std::setprecision(1) << skipRatio << "%\n";
    std::cout << "  Post time: " << postDuration << "us\n\n";

    // Most notifies should be skipped under high load
    EXPECT_GT(skipRatio, 50.0) << "High load scenario should skip most notifies";
}
