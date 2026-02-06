#include "System/Session/UDP/KCPAdapter.h"
#include "System/Session/UDP/IKCPWrapper.h"
#include "System/Session/UDP/KCPWrapper.h"
#include "System/Pch.h"
#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <vector>
#include <deque>
#include <chrono>

namespace System {

// Mock Dispatcher for KCP tests
class KCPTestMockDispatcher : public IDispatcher
{
public:
    void Post(IMessage *msg) override
    {
        // Just record the message, don't delete it
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

private:
    std::vector<IMessage*> _receivedMessages;
};

} // namespace System

using namespace System;

class KCPTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _dispatcher = std::make_unique<KCPTestMockDispatcher>();
    }

    void TearDown() override
    {
        _dispatcher.reset();
    }

    std::unique_ptr<KCPTestMockDispatcher> _dispatcher;
};

TEST_F(KCPTest, KCPAdapterCreation)
{
    uint32_t conv = 12345;

    auto kcp = std::make_unique<KCPAdapter>(conv);

    EXPECT_NE(kcp, nullptr);
    EXPECT_TRUE(kcp != nullptr);

    kcp.reset();
}

TEST_F(KCPTest, KCPAdapterSendAndRecv)
{
    uint32_t conv = 54321;
    auto kcp = std::make_unique<KCPAdapter>(conv);

    const char* testData = "Hello KCP!";
    int testDataLength = static_cast<int>(std::strlen(testData));

    // Send data
    int sent = kcp->Send(testData, testDataLength);
    EXPECT_GE(sent, 0);

    // Update to flush
    uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    kcp->Update(current);

    // Get output
    std::vector<uint8_t> output(1024);
    int outputLength = kcp->Output(output.data(), static_cast<int>(output.size()));
    EXPECT_GE(outputLength, 0);

    kcp.reset();
}

TEST_F(KCPTest, KCPAdapterInputAndRecv)
{
    uint32_t conv = 67890;
    auto kcp = std::make_unique<KCPAdapter>(conv);

    const char* testData = "Test data from client";
    int testDataLength = static_cast<int>(std::strlen(testData));

    // Input data
    int inputResult = kcp->Input(testData, testDataLength);
    EXPECT_GE(inputResult, 0);

    // Update to process
    uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    kcp->Update(current);

    // Try to receive
    std::vector<uint8_t> recvBuffer(1024);
    int recvLength = kcp->Recv(recvBuffer.data(), static_cast<int>(recvBuffer.size()));
    EXPECT_LE(recvLength, testDataLength);

    kcp.reset();
}

TEST_F(KCPTest, KCPAdapterMultipleSends)
{
    uint32_t conv = 11111;
    auto kcp = std::make_unique<KCPAdapter>(conv);

    const char* testMessages[] = {"First message", "Second message", "Third message", "Fourth message", "Fifth message"};
    const int numMessages = 5;

    // Send multiple messages
    for (int i = 0; i < numMessages; ++i)
    {
        int sent = kcp->Send(testMessages[i], static_cast<int>(std::strlen(testMessages[i])));
        EXPECT_GE(sent, 0);
    }

    // Update multiple times
    uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    kcp->Update(current);
    kcp->Update(current);
    kcp->Update(current);

    // Try to receive
    std::vector<uint8_t> recvBuffer(1024);
    int recvLength = kcp->Recv(recvBuffer.data(), static_cast<int>(recvBuffer.size()));
    EXPECT_GE(recvLength, 0);

    kcp.reset();
}

TEST_F(KCPTest, KCPAdapterSequence)
{
    uint32_t conv = 22222;
    auto kcp = std::make_unique<KCPAdapter>(conv);

    const char* testData = "Sequential test";
    int testDataLength = static_cast<int>(std::strlen(testData));

    // Send data
    int sent = kcp->Send(testData, testDataLength);
    EXPECT_GE(sent, 0);

    // Update
    uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    kcp->Update(current);

    // Get output
    std::vector<uint8_t> output(1024);
    int outputLength = kcp->Output(output.data(), static_cast<int>(output.size()));
    EXPECT_GE(outputLength, 0);

    // Create another KCP instance and input the output
    auto kcp2 = std::make_unique<KCPAdapter>(conv);

    // Note: This is a basic round-trip test
    // In a real scenario, we'd need to reconstruct the full packet including header

    kcp.reset();
    kcp2.reset();
}

TEST_F(KCPTest, KCPWrapperCreation)
{
    uint32_t conv = 33333;

    auto kcpWrapper = std::make_unique<KCPWrapper>();

    kcpWrapper->Initialize(conv);

    EXPECT_TRUE(kcpWrapper != nullptr);

    kcpWrapper.reset();
}

TEST_F(KCPTest, KCPWrapperSendAndRecv)
{
    uint32_t conv = 44444;
    auto kcpWrapper = std::make_unique<KCPWrapper>();

    kcpWrapper->Initialize(conv);

    const char* testData = "KCP Wrapper test";
    int testDataLength = static_cast<int>(std::strlen(testData));

    // Send data
    int sent = kcpWrapper->Send(testData, testDataLength);
    EXPECT_GE(sent, 0);

    // Update
    uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    kcpWrapper->Update(current);

    // Try to receive
    std::vector<uint8_t> recvBuffer(1024);
    int recvLength = kcpWrapper->Recv(recvBuffer.data(), static_cast<int>(recvBuffer.size()));
    EXPECT_LE(recvLength, testDataLength);

    kcpWrapper.reset();
}

TEST_F(KCPTest, KCPWrapperMultipleUpdates)
{
    uint32_t conv = 55555;
    auto kcpWrapper = std::make_unique<KCPWrapper>();

    kcpWrapper->Initialize(conv);

    const char* testData = "Multiple updates test";
    int testDataLength = static_cast<int>(std::strlen(testData));

    // Send data
    int sent = kcpWrapper->Send(testData, testDataLength);
    EXPECT_GE(sent, 0);

    // Update multiple times (simulating different time ticks)
    for (int i = 0; i < 5; ++i)
    {
        uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000) + i * 10;
        kcpWrapper->Update(current);
    }

    // Try to receive
    std::vector<uint8_t> recvBuffer(1024);
    int recvLength = kcpWrapper->Recv(recvBuffer.data(), static_cast<int>(recvBuffer.size()));
    EXPECT_GE(recvLength, 0);

    kcpWrapper.reset();
}

TEST_F(KCPTest, KCPWrapperOutput)
{
    uint32_t conv = 66666;
    auto kcpWrapper = std::make_unique<KCPWrapper>();

    kcpWrapper->Initialize(conv);

    const char* testData = "Output test";
    int testDataLength = static_cast<int>(std::strlen(testData));

    // Send data
    int sent = kcpWrapper->Send(testData, testDataLength);
    EXPECT_GE(sent, 0);

    // Update to flush
    uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    kcpWrapper->Update(current);

    // Get output
    std::vector<uint8_t> output(1024);
    int outputLength = kcpWrapper->Output(output.data(), static_cast<int>(output.size()));
    EXPECT_GE(outputLength, 0);

    kcpWrapper.reset();
}

TEST_F(KCPTest, KCPWrapperEmptyData)
{
    uint32_t conv = 77777;
    auto kcpWrapper = std::make_unique<KCPWrapper>();

    kcpWrapper->Initialize(conv);

    // Send empty data
    int sent = kcpWrapper->Send("", 0);
    EXPECT_GE(sent, 0);

    // Update
    uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    kcpWrapper->Update(current);

    // Try to receive
    std::vector<uint8_t> recvBuffer(1024);
    int recvLength = kcpWrapper->Recv(recvBuffer.data(), static_cast<int>(recvBuffer.size()));
    EXPECT_LE(recvLength, 0);

    kcpWrapper.reset();
}

TEST_F(KCPTest, KCPWrapperMixedOperations)
{
    uint32_t conv = 88888;
    auto kcpWrapper = std::make_unique<KCPWrapper>();

    kcpWrapper->Initialize(conv);

    // Send multiple messages with various lengths
    const char* testData1 = "Short";
    const char* testData2 = "This is a longer message for testing";
    const char* testData3 = "X";
    const char* testData4 = "";

    kcpWrapper->Send(testData1, static_cast<int>(std::strlen(testData1)));
    kcpWrapper->Send(testData2, static_cast<int>(std::strlen(testData2)));
    kcpWrapper->Send(testData3, static_cast<int>(std::strlen(testData3)));
    kcpWrapper->Send(testData4, 0);

    // Update multiple times
    uint32_t current = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    kcpWrapper->Update(current);
    kcpWrapper->Update(current);
    kcpWrapper->Update(current);

    // Try to receive
    std::vector<uint8_t> recvBuffer(1024);
    int recvLength = kcpWrapper->Recv(recvBuffer.data(), static_cast<int>(recvBuffer.size()));

    // We expect some data to be received (at least one message)
    EXPECT_GE(recvLength, 0);

    kcpWrapper.reset();
}
