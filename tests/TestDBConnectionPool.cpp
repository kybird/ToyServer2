#include "System/Database/DBConnectionPool.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace System;
using ::testing::Return;

// Mock Connection
class MockConnection : public IDatabaseConnection
{
public:
    MOCK_METHOD(bool, Connect, (const std::string &), (override));
    MOCK_METHOD(void, Disconnect, (), (override));
    MOCK_METHOD(bool, IsConnected, (), (override));
    MOCK_METHOD(bool, Ping, (), (override));
    MOCK_METHOD(bool, Execute, (const std::string &), (override));
    MOCK_METHOD(std::shared_ptr<IResultSet>, Query, (const std::string &), (override));
};

TEST(Sanity, Check)
{
    EXPECT_TRUE(true);
}

TEST(DBConnectionPoolTest, BasicPooling)
{
    // Factory creating MockConnection
    DBConnectionPool::ConnectionFactory factory = []()
    {
        MockConnection *mock = new MockConnection();
        // Setup default behavior
        EXPECT_CALL(*mock, Connect(testing::_)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock, IsConnected()).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock, Ping()).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock, Disconnect()).Times(testing::AtLeast(0));
        return mock;
    };

    // Pool size 2
    DBConnectionPool pool(2, "server=localhost", factory);
    pool.Init();

    // Acquire 1
    IDatabaseConnection *conn1 = pool.Acquire();
    ASSERT_NE(conn1, nullptr);

    // Acquire 2
    IDatabaseConnection *conn2 = pool.Acquire();
    ASSERT_NE(conn2, nullptr);

    // Acquire 3 (Should fail)
    IDatabaseConnection *conn3 = pool.Acquire();
    EXPECT_EQ(conn3, nullptr);

    // Release 1
    pool.Release(conn1);

    // Acquire again (Should get one)
    IDatabaseConnection *conn4 = pool.Acquire();
    EXPECT_EQ(conn4, conn1); // Reused
}

TEST(DBConnectionPoolTest, ReconnectOnFailure)
{
    // Factory
    DBConnectionPool::ConnectionFactory factory = []()
    {
        MockConnection *mock = new MockConnection();
        // Initial connect success
        EXPECT_CALL(*mock, Connect(testing::_)).WillRepeatedly(Return(true));
        // Disconnect called during reconnect
        EXPECT_CALL(*mock, Disconnect()).Times(testing::AtLeast(0));
        return mock;
    };

    DBConnectionPool pool(1, "conn", factory);
    pool.Init();

    IDatabaseConnection *conn = pool.Acquire();
    ASSERT_NE(conn, nullptr);
    MockConnection *mock = static_cast<MockConnection *>(conn);

    // Simulate broken connection
    // When checks happen inside Acquire (or next Acquire?), logic handles it?
    // Current Acquire logic checks Ping() immediately after pop.
    // So if we Release it back, then Acquire again, it should trigger check.

    EXPECT_CALL(*mock, IsConnected()).WillOnce(Return(false)); // Simulating disconnect
    // It should try Disconnect() then Connect() again
    EXPECT_CALL(*mock, Connect(testing::_)).WillOnce(Return(true)); // Reconnect success

    pool.Release(conn);

    // Acquire again -> triggers health check
    IDatabaseConnection *connNew = pool.Acquire();
    EXPECT_EQ(connNew, conn); // Same object, reconnected
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
