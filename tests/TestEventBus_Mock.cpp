#include "System/Dispatcher/IDispatcher.h"
#include "System/Events/EventBus.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;

// Mock Dispatcher
class MockDispatcher : public System::IDispatcher
{
public:
    MOCK_METHOD(void, Post, (System::IMessage *), (override));
    MOCK_METHOD(bool, Process, (), (override));
    MOCK_METHOD(size_t, GetQueueSize, (), (const, override));
    MOCK_METHOD(bool, IsOverloaded, (), (const, override));
    MOCK_METHOD(bool, IsRecovered, (), (const, override));
    MOCK_METHOD(void, RegisterTimerHandler, (System::ITimerHandler *), (override));
    MOCK_METHOD(void, Wait, (int), (override));

    // Target Method
    MOCK_METHOD(void, Push, (std::function<void()>), (override));
    MOCK_METHOD(void, WithSession, (uint64_t, std::function<void(System::SessionContext &)>), (override));
};

// Test Events
struct TestLoginEvent
{
    int userId;
};

TEST(EventBusSanity, Check)
{
    EXPECT_TRUE(true);
}

TEST(EventBusTest, Publish_Should_Push_To_Dispatcher)
{
    System::EventBus::Instance().Reset();
    MockDispatcher mockDispatcher;
    bool handlerCalled = false;

    // 1. Subscribe
    System::EventBus::Instance().Subscribe<TestLoginEvent>(
        &mockDispatcher,
        [&](const TestLoginEvent &e)
        {
            handlerCalled = true;
            EXPECT_EQ(e.userId, 100);
        }
    );

    // 2. Expect Push to be called once
    // Note: We don't execute the lambda here, just verify Push is called.
    EXPECT_CALL(mockDispatcher, Push(_)).Times(1);

    // 3. Publish
    System::EventBus::Instance().Publish(TestLoginEvent{100});
}

TEST(EventBusTest, MultipleSubscribers)
{
    System::EventBus::Instance().Reset();
    MockDispatcher mock1;
    MockDispatcher mock2;

    System::EventBus::Instance().Subscribe<TestLoginEvent>(
        &mock1,
        [](const TestLoginEvent &)
        {
        }
    );
    System::EventBus::Instance().Subscribe<TestLoginEvent>(
        &mock2,
        [](const TestLoginEvent &)
        {
        }
    );

    EXPECT_CALL(mock1, Push(_)).Times(1);
    EXPECT_CALL(mock2, Push(_)).Times(1);

    System::EventBus::Instance().Publish(TestLoginEvent{200});
}
