#include "System/Dispatcher/MessagePool.h"
#include <gtest/gtest.h>
#include <vector>

using namespace System;

class MessagePoolExpansionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MessagePool::Prepare(100);
    }

    void TearDown() override
    {
        MessagePool::Clear();
    }
};

TEST_F(MessagePoolExpansionTest, PacketAllocationStrategy)
{
    // 1. 소형 패킷 (4KB 이하) - 풀링 확인
    {
        PacketMessage *msg = MessagePool::AllocatePacket(1024);
        ASSERT_NE(msg, nullptr);
        EXPECT_EQ(msg->length, 1024);
        EXPECT_TRUE(msg->isPooled);
        MessagePool::Free(msg);
    }

    // 2. 대형 패킷 (4KB 초과) - 힙 할당 확인
    {
        const uint16_t LARGE_SIZE = 8192; // 8KB
        PacketMessage *msg = MessagePool::AllocatePacket(LARGE_SIZE);
        ASSERT_NE(msg, nullptr);
        EXPECT_EQ(msg->length, LARGE_SIZE);
        EXPECT_FALSE(msg->isPooled);

        // 데이터 쓰기 테스트 (메모리 경계 확인)
        std::memset(msg->Payload(), 0xCC, LARGE_SIZE);
        EXPECT_EQ(msg->Payload()[LARGE_SIZE - 1], 0xCC);

        MessagePool::Free(msg);
    }

    // 3. 초거대 패킷 (1MB) - 퀘스트 정보 등 시뮬레이션
    {
        const uint16_t HUGE_SIZE = 60000; // uint16_t 최대 근처 (제약 조건 확인 필요시 상향)
        PacketMessage *msg = MessagePool::AllocatePacket(HUGE_SIZE);
        ASSERT_NE(msg, nullptr);
        EXPECT_FALSE(msg->isPooled);
        MessagePool::Free(msg);
    }
}

TEST_F(MessagePoolExpansionTest, LambdaAllocation)
{
    // 람다 메시지는 항상 isPooled = false 여야 함 (벤치마크 기반 결정)
    // DispatcherImpl::Push 내부 시뮬레이션
    LambdaMessage *msg = new LambdaMessage();
    msg->task = []()
    {
    };
    EXPECT_FALSE(msg->isPooled);
    MessagePool::Free(msg); // delete msg가 내부에서 호출되어야 함
}
