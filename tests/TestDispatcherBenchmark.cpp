#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ISession.h"
#include "System/Session/SessionContext.h"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>

using namespace System;

// 더미 핸들러
class MockPacketHandler : public IPacketHandler
{
public:
    void HandlePacket(SessionContext ctx, PacketView packet) override
    {
    }
    void OnSessionDisconnect(SessionContext ctx) override
    {
    }
};

class DispatcherBenchmark : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MessagePool::Prepare(10000);
    }
};

TEST_F(DispatcherBenchmark, SmartNotifyLoadTest)
{
    auto handler = std::make_shared<MockPacketHandler>();
    DispatcherImpl dispatcher(handler);

    const int messageCount = 100000;
    const int producerCount = 4;

    // 로직 스레드 시뮬레이션
    std::atomic<bool> running{true};
    std::atomic<int> processedCount{0};

    std::thread consumer(
        [&]()
        {
            while (running || dispatcher.GetQueueSize() > 0)
            {
                if (!dispatcher.Process())
                {
                    dispatcher.Wait(1);
                }
                else
                {
                    processedCount.fetch_add(1); // 대략적인 처리 횟수 (벌크 단위)
                }
            }
        }
    );

    auto start = std::chrono::high_resolution_clock::now();

    // 여러 스레드에서 동시에 Post 실행 (고부하 상황)
    std::vector<std::thread> producers;
    for (int p = 0; p < producerCount; ++p)
    {
        producers.emplace_back(
            [&]()
            {
                for (int i = 0; i < messageCount / producerCount; ++i)
                {
                    // 람다 메시지 풀링 사용 테스트도 겸함
                    dispatcher.Push(
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

    std::cout << "[Dispatcher] [Optimized] Processed " << messageCount << " messages in " << duration << "ms"
              << std::endl;
}

// 최적화 전 시뮬레이션: 매번 notify 수행 + 매번 new/delete (메시지 풀 미사용)
TEST_F(DispatcherBenchmark, LegacyLoadTest)
{
    auto handler = std::make_shared<MockPacketHandler>();
    DispatcherImpl dispatcher(handler);

    const int messageCount = 100000;
    const int producerCount = 4;

    std::atomic<bool> running{true};
    std::thread consumer(
        [&]()
        {
            while (running || dispatcher.GetQueueSize() > 0)
            {
                if (!dispatcher.Process())
                {
                    dispatcher.Wait(1);
                }
            }
        }
    );

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    for (int p = 0; p < producerCount; ++p)
    {
        producers.emplace_back(
            [&]()
            {
                for (int i = 0; i < messageCount / producerCount; ++i)
                {
                    // 레거시 시뮬레이션: MessagePool 대신 new 사용
                    LambdaMessage *msg = new LambdaMessage();
                    msg->task = []()
                    {
                    };

                    // DispatcherImpl 내부적으로 notify가 최적화되어 있으므로,
                    // 레거시 성능은 주로 new/delete 오버헤드 위주로 측정됨
                    // (코드를 되돌리지 않고도 할당 오버헤드 비교 가능)
                    dispatcher.Post(msg);
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

    std::cout << "[Dispatcher] [Legacy-Sim] Processed " << messageCount << " messages in " << duration << "ms"
              << std::endl;
}
