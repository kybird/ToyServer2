#include "System/Network/UDPEndpointRegistry.h"
#include "System/Session/UDPSession.h"
#include "System/Dispatcher/IDispatcher.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>

namespace System {

// Mock Session for testing
class TestUDPSession : public UDPSession
{
public:
    TestUDPSession() = default;
    virtual ~TestUDPSession() = default;

    void SetEndpoint(const boost::asio::ip::udp::endpoint &ep)
    {
        _impl->endpoint = ep;
        _impl->lastActivity = std::chrono::steady_clock::now();
    }

    uint64_t GetLastActivityMs() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - _impl->lastActivity
        ).count();
    }
};

// Mock Dispatcher for testing
class TestUDPSessionMockDispatcher : public IDispatcher
{
public:
    void Post(IMessage *msg) override
    {
        // Record the message
        _receivedMessages.push_back(msg);
    }

    bool Process() override { return false; }
    void Wait(int) override {}
    size_t GetQueueSize() const override { return 0; }
    bool IsOverloaded() const override { return false; }
    bool IsRecovered() const override { return true; }
    void RegisterTimerHandler(ITimerHandler *) override {}
    void WithSession(uint64_t, std::function<void(SessionContext &)>) override {}
    void Shutdown() override {}
};

} // namespace System

using namespace System;

class UDPEndpointRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _registry = std::make_unique<UDPEndpointRegistry>();
        _dispatcher = std::make_unique<TestUDPSessionMockDispatcher>();
    }

    void TearDown() override
    {
        _registry.reset();
        _dispatcher.reset();
    }

    std::unique_ptr<UDPEndpointRegistry> _registry;
    std::unique_ptr<TestUDPSessionMockDispatcher> _dispatcher;
};

TEST_F(UDPEndpointRegistryTest, RegisterAndFind)
{
    auto session = std::make_unique<TestUDPSession>();
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"),
        12345
    );
    session->SetEndpoint(endpoint);

    // Register session
    _registry->Register(endpoint, session.get());

    // Find session
    auto found = _registry->Find(endpoint);
    ASSERT_NE(found, nullptr);
    ASSERT_EQ(found, session.get());

    // Verify endpoint matches
    auto* udpSession = static_cast<TestUDPSession*>(found);
    ASSERT_EQ(udpSession->GetEndpoint(), endpoint);
}

TEST_F(UDPEndpointRegistryTest, RegisterAndUpdate)
{
    auto session1 = std::make_unique<TestUDPSession>();
    auto session2 = std::make_unique<TestUDPSession>();
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.2"),
        54321
    );
    session1->SetEndpoint(endpoint);
    session2->SetEndpoint(endpoint);

    // Register first session
    _registry->Register(endpoint, session1.get());

    // Update with second session
    _registry->Register(endpoint, session2.get());

    // Find should return second session
    auto found = _registry->Find(endpoint);
    ASSERT_NE(found, nullptr);
    ASSERT_EQ(found, session2.get());

    // Verify endpoint still matches
    auto* udpSession = static_cast<TestUDPSession*>(found);
    ASSERT_EQ(udpSession->GetEndpoint(), endpoint);
}

TEST_F(UDPEndpointRegistryTest, RemoveSession)
{
    auto session = std::make_unique<TestUDPSession>();
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.3"),
        11111
    );
    session->SetEndpoint(endpoint);

    // Register session
    _registry->Register(endpoint, session.get());

    // Verify it exists
    auto found = _registry->Find(endpoint);
    ASSERT_NE(found, nullptr);

    // Remove session
    _registry->Remove(endpoint);

    // Verify it's gone
    found = _registry->Find(endpoint);
    ASSERT_EQ(found, nullptr);
}

TEST_F(UDPEndpointRegistryTest, RemoveNonExistentEndpoint)
{
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.4"),
        99999
    );

    // Should not throw
    EXPECT_NO_THROW(_registry->Remove(endpoint));
}

TEST_F(UDPEndpointRegistryTest, UpdateActivity)
{
    auto session = std::make_unique<TestUDPSession>();
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.5"),
        22222
    );
    session->SetEndpoint(endpoint);

    // Register session
    _registry->Register(endpoint, session.get());

    // Get initial activity
    auto initialActivity = session->GetLastActivityMs();

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Update activity
    _registry->UpdateActivity(endpoint);

    // Get updated activity
    auto updatedActivity = session->GetLastActivityMs();

    // Should be approximately 10ms more (allowing for scheduling variance)
    EXPECT_GE(updatedActivity - initialActivity, 8);
    EXPECT_LE(updatedActivity - initialActivity, 20);
}

TEST_F(UDPEndpointRegistryTest, CleanupTimeouts)
{
    auto session1 = std::make_unique<TestUDPSession>();
    auto session2 = std::make_unique<TestUDPSession>();
    auto session3 = std::make_unique<TestUDPSession>();

    auto endpoint1 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.6"),
        33333
    );
    auto endpoint2 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.7"),
        44444
    );
    auto endpoint3 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.8"),
        55555
    );

    session1->SetEndpoint(endpoint1);
    session2->SetEndpoint(endpoint2);
    session3->SetEndpoint(endpoint3);

    _registry->Register(endpoint1, session1.get());
    _registry->Register(endpoint2, session2.get());
    _registry->Register(endpoint3, session3.get());

    // Verify all sessions are present
    ASSERT_NE(_registry->Find(endpoint1), nullptr);
    ASSERT_NE(_registry->Find(endpoint2), nullptr);
    ASSERT_NE(_registry->Find(endpoint3), nullptr);

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Remove middle session
    _registry->Remove(endpoint2);

    // Cleanup with 15ms timeout (should remove session1 but keep session3)
    auto removed = _registry->CleanupTimeouts(15);

    // Should have removed 1 session
    EXPECT_EQ(removed, 1u);

    // Verify session1 is removed
    ASSERT_EQ(_registry->Find(endpoint1), nullptr);

    // Verify session3 is still there
    ASSERT_NE(_registry->Find(endpoint3), nullptr);
}

TEST_F(UDPEndpointRegistryTest, CleanupAllTimeouts)
{
    auto session1 = std::make_unique<TestUDPSession>();
    auto session2 = std::make_unique<TestUDPSession>();

    auto endpoint1 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.9"),
        66666
    );
    auto endpoint2 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.10"),
        77777
    );

    session1->SetEndpoint(endpoint1);
    session2->SetEndpoint(endpoint2);

    _registry->Register(endpoint1, session1.get());
    _registry->Register(endpoint2, session2.get());

    // Wait enough time for both to timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Cleanup with 20ms timeout
    auto removed = _registry->CleanupTimeouts(20);

    // Should have removed 2 sessions
    EXPECT_EQ(removed, 2u);

    // Verify both are gone
    ASSERT_EQ(_registry->Find(endpoint1), nullptr);
    ASSERT_EQ(_registry->Find(endpoint2), nullptr);
}

TEST_F(UDPEndpointRegistryTest, FindNonExistent)
{
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.11"),
        88888
    );

    auto found = _registry->Find(endpoint);
    ASSERT_EQ(found, nullptr);
}

TEST_F(UDPEndpointRegistryTest, ThreadSafety)
{
    const int numThreads = 10;
    const int opsPerThread = 100;
    std::vector<std::thread> threads;

    auto session = std::make_unique<TestUDPSession>();
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string("127.0.0.12"),
        99999
    );
    session->SetEndpoint(endpoint);

    // Start threads that will modify registry concurrently
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, &endpoint, i] {
            for (int j = 0; j < opsPerThread; ++j)
            {
                auto tempSession = std::make_unique<TestUDPSession>();
                tempSession->SetEndpoint(endpoint);

                _registry->Register(endpoint, tempSession.get());
                _registry->Find(endpoint);
                _registry->UpdateActivity(endpoint);
                _registry->Remove(endpoint);
                _registry->Register(endpoint, tempSession.get());
            }
        });
    }

    // Wait for all threads to complete
    for (auto &t : threads)
    {
        t.join();
    }

    // Verify registry is still usable
    auto session2 = std::make_unique<TestUDPSession>();
    session2->SetEndpoint(endpoint);
    _registry->Register(endpoint, session2.get());

    auto found = _registry->Find(endpoint);
    ASSERT_NE(found, nullptr);
    ASSERT_EQ(found, session2.get());
}
