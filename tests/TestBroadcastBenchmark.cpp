#include "System/Dispatcher/MessagePool.h"
#include "System/Packet/PacketBroadcast.h"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

using namespace System;

class BroadcastBenchmark : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 프리젠테이션용 데이터 준비
        MessagePool::Prepare(2000);
    }
};

TEST_F(BroadcastBenchmark, CompareLegacyVsOptimized)
{
    const int sessionCount = 1000;
    const int packetSize = 1024; // 1KB
    const int iterations = 100;

    // 1. Legacy 방식 (시뮬레이션: 세션마다 할당 및 복사 발생)
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            std::vector<PacketMessage *> packets;
            packets.reserve(sessionCount);

            for (int s = 0; s < sessionCount; ++s)
            {
                PacketMessage *msg = MessagePool::AllocatePacket(packetSize);
                // 실제 전송 로직 대신 루프 내에서 할당/복사 비용 측정
                std::memset(msg->Payload(), 0xAF, packetSize);
                packets.push_back(msg);
            }

            for (auto msg : packets)
            {
                MessagePool::Free(msg);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[Legacy] " << sessionCount << " sessions, " << iterations << " iterations: " << duration << "ms"
                  << std::endl;
    }

    // 2. Optimized 방식 (참조 카운팅: 할당 1회, 복사 0회)
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            // 원본 1개 생성
            PacketMessage *original = MessagePool::AllocatePacket(packetSize);
            std::memset(original->Payload(), 0xAF, packetSize);

            for (int s = 0; s < sessionCount; ++s)
            {
                // 참조 카운트만 증가 (복사 없음)
                original->AddRef();
                // 실제 전송 큐에 넣는 대신 DecRef 호출로 시뮬레이션 (Flush 단계에서 Free 호출됨)
                MessagePool::Free(original);
            }

            // 원본 해제 (마지막 참조 해제)
            MessagePool::Free(original);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[Optimized] " << sessionCount << " sessions, " << iterations << " iterations: " << duration
                  << "ms" << std::endl;
    }
}
