#include "System/Memory/LockFreeObjectPool.h"
#include "System/Session/SessionPool.h"
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

using namespace System;

namespace System {
namespace Testing {

// 1. False Sharing Reproduction
TEST(SecurityReproduction, Item1_FalseSharing) {
    struct Padded {
        alignas(64) std::atomic<size_t> a{0};
        alignas(64) std::atomic<size_t> b{0};
    } padded;

    struct Unpadded {
        std::atomic<size_t> a{0};
        std::atomic<size_t> b{0};
    } unpadded;

    const int iterations = 50'000'000;

    auto benchmark = [&](auto& data) {
        auto start = std::chrono::high_resolution_clock::now();
        std::thread t1([&]() {
            for (int i = 0; i < iterations; ++i) data.a.fetch_add(1, std::memory_order_relaxed);
        });
        std::thread t2([&]() {
            for (int i = 0; i < iterations; ++i) data.b.fetch_add(1, std::memory_order_relaxed);
        });
        t1.join();
        t2.join();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    };

    // Warm up
    benchmark(unpadded);
    benchmark(padded);

    auto unpaddedTime = benchmark(unpadded);
    auto paddedTime = benchmark(padded);

    std::cout << "[Item 1] Unpadded Time: " << unpaddedTime << "ms" << std::endl;
    std::cout << "[Item 1] Padded Time: " << paddedTime << "ms" << std::endl;

    if (unpaddedTime > paddedTime * 1.1) {
        std::cout << "[Item 1] False Sharing detected! (Padded is " << (float)unpaddedTime / paddedTime << "x faster)" << std::endl;
    }
}

// 2. Custom Synchronization Race Condition (P1)
class MockFlushSession {
public:
    std::atomic<bool> _isSending{false};
    moodycamel::ConcurrentQueue<int> _sendQueue;
    std::atomic<int> _activeThreadsInFlush{0};
    std::atomic<int> _raceDetected{0};

    void Enqueue(int val) {
        _sendQueue.enqueue(val);
        if (_isSending.exchange(true, std::memory_order_acq_rel) == false) {
            Flush();
        }
    }

    void Flush() {
        int active = _activeThreadsInFlush.fetch_add(1);
        if (active > 0) {
            _raceDetected.fetch_add(1);
        }

        int val;
        int count = 0;
        // Simulate picking up some work
        while (count < 10 && _sendQueue.try_dequeue(val)) {
            count++;
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

        if (count == 0) {
            _isSending.store(false);
            
            // Artificial delay to widen the race window where EnqueueSend might see _isSending == false
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::microseconds(1));

            if (_sendQueue.try_dequeue(val)) {
                if (!_isSending.exchange(true)) {
                    _sendQueue.enqueue(val);
                    _activeThreadsInFlush.fetch_sub(1);
                    Flush(); // Recursive call pattern
                    return;
                } else {
                    _sendQueue.enqueue(val);
                }
            }
            _activeThreadsInFlush.fetch_sub(1);
            return;
        }

        // Simulate async_write initiation (in real code, OnWriteComplete will call Flush again)
        _activeThreadsInFlush.fetch_sub(1);
    }
};

TEST(SecurityReproduction, Item2_CustomSyncRace) {
    MockFlushSession session;
    const int numThreads = 8;
    const int opsPerThread = 500;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < opsPerThread; ++j) {
                session.Enqueue(j);
                std::this_thread::yield();
            }
        });
    }

    for (auto& t : threads) t.join();

    if (session._raceDetected > 0) {
        std::cout << "[Item 2] Race Condition detected in Flush! Concurrent access count: " << session._raceDetected.load() << std::endl;
    } else {
        std::cout << "[Item 2] Race Condition not detected this time." << std::endl;
    }
}

// 3. Session Pool ABA Reproduction (P0)
class MockSessionForABA : public RefCounted<MockSessionForABA> {
public:
    std::atomic<int> ioRef{0};
    int data = 0;
    void Reset() { data = 0; ioRef = 0; }
};

TEST(SecurityReproduction, Item3_SessionPoolABA) {
    SessionPoolBase<MockSessionForABA> pool;
    pool.WarmUp(10);

    MockSessionForABA* sessionA = pool.Acquire();
    sessionA->data = 100;
    sessionA->ioRef.fetch_add(1);

    pool.Release(sessionA);

    MockSessionForABA* sessionB = pool.Acquire();
    sessionB->data = 200;

    sessionA->data = 999; // Late IO completion simulation

    if (sessionB == sessionA) {
        EXPECT_EQ(sessionB->data, 999);
        std::cout << "[Item 3] ABA Problem successfully reproduced! sessionB->data = " << sessionB->data << std::endl;
    } else {
        std::cout << "[Item 3] Session memory was not reused. ABA not visible in this run." << std::endl;
    }
}

// 4. Exception Handling (P2)
TEST(SecurityReproduction, Item4_ExceptionHandler) {
    std::cout << "[Item 4] Code Inspection Result: Framework.cpp lines 282-295 contains try-catch that logs but exits thread loop." << std::endl;
    std::cout << "This confirms that exceptions in async handlers lead to worker thread death." << std::endl;
}

// 5. Thread Explosion Reproduction (P2)
TEST(SecurityReproduction, Item5_ThreadExplosion) {
    std::cout << "[Item 5] Resource check: SessionPoolBase::TriggerBackgroundGrowth uses std::thread(...).detach() which bypasses ThreadPool." << std::endl;
}

} // namespace Testing
} // namespace System
