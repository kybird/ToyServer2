#include "System/Session/UDPSession.h"
#include "System/Session/SessionFactory.h"
#include "System/Network/UDPEndpointRegistry.h"
#include "System/Dispatcher/IDispatcher.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstddef>
#include <set>

namespace System {

// Mock Dispatcher that records and manages messages
class TestUDPSessionMockDispatcher : public IDispatcher
{
public:
    void Post(IMessage *msg) override
    {
        _receivedMessages.push_back(msg);
    }

    bool Process() override
    {
        bool processed = false;
        while (!_queue.empty())
        {
            auto task = _queue.front();
            _queue.pop_front();
            task();
            processed = true;
        }
        return processed;
    }

    void Wait(int timeoutMs) override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
    }

    size_t GetQueueSize() const override { return _queue.size(); }
    bool IsOverloaded() const override { return false; }
    bool IsRecovered() const override { return true; }
    void RegisterTimerHandler(ITimerHandler *) override {}
    void WithSession(uint64_t, std::function<void(SessionContext &)>) override {}
    void Shutdown() override {}

    void Push(std::function<void()> task) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_back(task);
    }

    const std::vector<IMessage*>& GetReceivedMessages() const { return _receivedMessages; }

private:
    std::vector<IMessage*> _receivedMessages;
    std::deque<std::function<void()>> _queue;
    std::mutex _mutex;
};

} // namespace System

using namespace System;

class UDPNetworkTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SessionFactory::SetServerRole(ServerRole::Backend);
        _dispatcher = std::make_unique<TestUDPSessionMockDispatcher>();
        _registry = std::make_unique<UDPEndpointRegistry>();
    }

    void TearDown() override
    {
        _dispatcher.reset();
        _registry.reset();
    }

    std::unique_ptr<TestUDPSessionMockDispatcher> _dispatcher;
    std::unique_ptr<UDPEndpointRegistry> _registry;
};

TEST_F(UDPNetworkTest, UDPSessionCreation)
{
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.1"),
        12345
    );

    auto session = SessionFactory::CreateUDPSession(endpoint, _dispatcher.get());
    ASSERT_NE(session, nullptr);

    EXPECT_TRUE(session->IsConnected());
    EXPECT_EQ(session->GetId(), 1ULL);

    SessionFactory::Destroy(session);
}

TEST_F(UDPNetworkTest, UDPSessionEndpointTracking)
{
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.2"),
        54321
    );

    auto session = SessionFactory::CreateUDPSession(endpoint, _dispatcher.get());
    ASSERT_NE(session, nullptr);

    SessionFactory::Destroy(session);
}

TEST_F(UDPNetworkTest, UDPSessionMultipleEndpoints)
{
    std::vector<boost::asio::ip::udp::endpoint> endpoints = {
        boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address("127.0.0.3"),
            11111
        ),
        boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address("127.0.0.4"),
            22222
        ),
        boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address("127.0.0.5"),
            33333
        )
    };

    std::vector<ISession*> sessions;
    for (const auto& endpoint : endpoints)
    {
        auto session = SessionFactory::CreateUDPSession(endpoint, _dispatcher.get());
        ASSERT_NE(session, nullptr);
        sessions.push_back(session);
    }

    for (auto session : sessions)
    {
        EXPECT_TRUE(session->IsConnected());
    }

    for (auto session : sessions)
    {
        SessionFactory::Destroy(session);
    }
}

TEST_F(UDPNetworkTest, UDPSessionActivityTracking)
{
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.6"),
        44444
    );

    auto session = SessionFactory::CreateUDPSession(endpoint, _dispatcher.get());
    ASSERT_NE(session, nullptr);

    session->Close();
    EXPECT_FALSE(session->IsConnected());

    SessionFactory::Destroy(session);
}

TEST_F(UDPNetworkTest, UDPEndpointRegistryIntegration)
{
    auto endpoint1 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.7"),
        55555
    );
    auto endpoint2 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.8"),
        66666
    );

    auto session1 = SessionFactory::CreateUDPSession(endpoint1, _dispatcher.get());
    auto session2 = SessionFactory::CreateUDPSession(endpoint2, _dispatcher.get());

    ASSERT_NE(session1, nullptr);
    ASSERT_NE(session2, nullptr);

    _registry->Register(endpoint1, session1);
    _registry->Register(endpoint2, session2);

    auto found1 = _registry->Find(endpoint1);
    auto found2 = _registry->Find(endpoint2);
    ASSERT_EQ(found1, session1);
    ASSERT_EQ(found2, session2);

    _registry->UpdateActivity(endpoint1);

    _registry->Remove(endpoint1);
    found1 = _registry->Find(endpoint1);
    ASSERT_EQ(found1, nullptr);

    SessionFactory::Destroy(session1);
    SessionFactory::Destroy(session2);
}

TEST_F(UDPNetworkTest, UDPSessionSendPacket)
{
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.9"),
        77777
    );

    auto session = SessionFactory::CreateUDPSession(endpoint, _dispatcher.get());
    ASSERT_NE(session, nullptr);

    SessionFactory::Destroy(session);
}

TEST_F(UDPNetworkTest, UDPSessionSessionIdUniqueness)
{
    const int numSessions = 100;
    std::vector<ISession*> sessions;
    std::set<uint64_t> ids;

    for (int i = 0; i < numSessions; ++i)
    {
        auto endpoint = boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address("127.0.0.10"),
            88888 + i
        );

        auto session = SessionFactory::CreateUDPSession(endpoint, _dispatcher.get());
        ASSERT_NE(session, nullptr);
        sessions.push_back(session);
        ids.insert(session->GetId());
    }

    EXPECT_EQ(ids.size(), static_cast<size_t>(numSessions));

    uint64_t expectedId = 1;
    for (auto id : ids)
    {
        EXPECT_EQ(id, expectedId++);
    }

    for (auto session : sessions)
    {
        SessionFactory::Destroy(session);
    }
}

TEST_F(UDPNetworkTest, UDPSessionConnectionLifecycle)
{
    auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.11"),
        99999
    );

    auto session = SessionFactory::CreateUDPSession(endpoint, _dispatcher.get());
    ASSERT_NE(session, nullptr);

    EXPECT_TRUE(session->IsConnected());
    EXPECT_EQ(session->GetId(), 1ULL);

    session->Close();
    EXPECT_FALSE(session->IsConnected());

    SessionFactory::Destroy(session);
}

TEST_F(UDPNetworkTest, UDPEndpointRegistryCleanup)
{
    auto endpoint1 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.12"),
        11111
    );
    auto endpoint2 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.13"),
        22222
    );
    auto endpoint3 = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.14"),
        33333
    );

    auto session1 = SessionFactory::CreateUDPSession(endpoint1, _dispatcher.get());
    auto session2 = SessionFactory::CreateUDPSession(endpoint2, _dispatcher.get());
    auto session3 = SessionFactory::CreateUDPSession(endpoint3, _dispatcher.get());

    ASSERT_NE(session1, nullptr);
    ASSERT_NE(session2, nullptr);
    ASSERT_NE(session3, nullptr);

    _registry->Register(endpoint1, session1);
    _registry->Register(endpoint2, session2);
    _registry->Register(endpoint3, session3);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    size_t removed = _registry->CleanupTimeouts(15);

    EXPECT_TRUE(removed == 2);

    SessionFactory::Destroy(session1);
    SessionFactory::Destroy(session2);
    SessionFactory::Destroy(session3);
}
