#include <boost/asio.hpp>
#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vector>


// [Convention] 시스템 모듈 내부 테스트이므로 구현체 참조가 허용됨
#include "System/Dispatcher/IDispatcher.h"
#include "System/Session/SessionFactory.h"
#include "System/Session/UDP/KCPWrapper.h"
#include "System/Session/UDPSession.h"

using namespace System;

// [Safety] 테스트 전체 생명주기 동안 살아있는 전역 디스패처
class GlobalMockDispatcher : public IDispatcher
{
public:
    void Post(IMessage *) override
    {
    }
    bool Process() override
    {
        return true;
    }
    void Wait(int) override
    {
    }
    size_t GetQueueSize() const override
    {
        return 0;
    }
    bool IsOverloaded() const override
    {
        return false;
    }
    bool IsRecovered() const override
    {
        return true;
    }
    void WithSession(uint64_t, std::function<void(SessionContext &)>) override
    {
    }
    void RegisterTimerHandler(ITimerHandler *) override
    {
    }
    void Push(std::function<void()>) override
    {
    }
    void Shutdown() override
    {
    }
};

static GlobalMockDispatcher g_dispatcher;

/**
 * @brief 비교군 테스트: 세션 풀링 vs 직접 할당
 */
TEST(PerformanceComparison, SessionManagement)
{
    spdlog::set_level(spdlog::level::off);
    boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 12345);
    const int iterations = 1000;

    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        ISession *session = SessionFactory::CreateUDPSession(endpoint, &g_dispatcher);
        if (session)
        {
            SessionFactory::Destroy(session);
        }
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> poolDuration = end1 - start1;

    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        UDPSession *session = new UDPSession();
        session->Reset(nullptr, i, &g_dispatcher, endpoint);
        delete session;
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> directDuration = end2 - start2;

    printf("\n[Comparison] Session Management Overhead (%d iterations)\n", iterations);
    printf(" - [Group A] Session Pooling: %.4f ms\n", poolDuration.count());
    printf(" - [Group B] Direct Allocation: %.4f ms\n", directDuration.count());
    fflush(stdout);
}

/**
 * @brief 비교군 테스트: KCP 프로토콜 vs Raw Data Copy
 */
TEST(PerformanceComparison, ProtocolOverhead)
{
    const int iterations = 10000;
    const int payloadSize = 512;
    std::vector<char> payload(payloadSize, 'A');
    uint8_t transportBuf[2048];
    uint8_t recvBuf[2048];

    KCPWrapper kcpSender;
    KCPWrapper kcpReceiver;
    kcpSender.Initialize(100);
    kcpReceiver.Initialize(100);

    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        kcpSender.Send(payload.data(), payloadSize);
        kcpSender.Update(0);
        int len = kcpSender.Output(transportBuf, sizeof(transportBuf));
        if (len > 0)
            kcpReceiver.Input(transportBuf, len);
        kcpReceiver.Update(0);
        while (kcpReceiver.Recv(recvBuf, sizeof(recvBuf)) > 0)
        {
        }
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> kcpDuration = end1 - start1;

    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        std::memcpy(transportBuf, payload.data(), payloadSize);
        std::memcpy(recvBuf, transportBuf, payloadSize);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> rawDuration = end2 - start2;

    printf("\n[Comparison] Protocol Processing Overhead (%d iterations)\n", iterations);
    printf(" - [Group A] KCP Wrapper: %.4f ms\n", kcpDuration.count());
    printf(" - [Group B] Raw Data Copy: %.4f ms\n", rawDuration.count());
    fflush(stdout);
}
