#include "System/MQ/MessageSystem.h"
#include "System/MQ/NatsDriver.h"
#include "System/MQ/RedisStreamDriver.h"
#include "System/Pch.h"
#include "System/Thread/ThreadPool.h"
#include <gtest/gtest.h>

// Note: These tests require running NATS and Redis servers to pass 'Connect' = true.
// In CI/Build environment without them, we expect them to handle failure gracefully or fail to connect.
// For now, we verify the code structure and optional connectivity.

TEST(MQ, NatsDriver_Lifecycle)
{
    System::MQ::NatsDriver driver;
    // We expect connection to fail if no server, but it shouldn't crash
    bool connected = driver.Connect("nats://localhost:4222");
    if (connected)
    {
        bool pub = driver.Publish("test.subject", "hello");
        EXPECT_TRUE(pub);
        driver.Disconnect();
    }
    else
    {
        SUCCEED() << "Skipping NATS functional test (No connection)";
    }
}

TEST(MQ, RedisStreamDriver_Lifecycle)
{
    System::MQ::RedisStreamDriver driver;
    System::ThreadPool pool(1, "TestMQ_Redis");
    pool.Start();
    driver.SetThreadPool(&pool);

    bool connected = driver.Connect("tcp://localhost:6379");
    if (connected)
    {
        bool pub = driver.Publish("test_stream", "hello_redis");
        EXPECT_TRUE(pub);
        driver.Disconnect();
    }
    else
    {
        SUCCEED() << "Skipping Redis functional test (No connection)";
    }
}

TEST(MQ, MessageSystem_Integration)
{
    // Basic singleton access check
    auto &sys = System::MQ::MessageSystem::Instance();
    std::shared_ptr<System::ThreadPool> pool = std::make_shared<System::ThreadPool>(1, "TestMQ_Sys");
    pool->Start();

    // Intentionally invalid or default strings
    sys.Initialize("nats://localhost:4222", "tcp://localhost:6379", pool.get());

    // Attempt publish (will fail if not connected, but safe)
    sys.Publish("any", "data", System::MQ::MessageQoS::Fast);

    sys.Shutdown();
}
