#include "../../Examples/VampireSurvivor/Protocol/game.pb.h"
#include "System/Packet/PacketBase.h"
#include "System/Pch.h"
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <vector>

using namespace Protocol;
using namespace System;

// [Test 1] NaN/Inf가 직렬화 시 크래시를 유발하는가?
TEST(CrashReproduction, Float_NaN_Inf_Safety)
{
    S_MoveObjectBatch pkt;
    // Protocol::S_MoveObjectBatch uses 'moves' field (repeated ObjectPos)
    auto *obj = pkt.add_moves();
    obj->set_object_id(100);
    obj->set_state(Protocol::ObjectState::MOVING);

    // [Inject] NaN and Infinity
    obj->set_x(std::numeric_limits<float>::quiet_NaN());
    obj->set_y(std::numeric_limits<float>::infinity());
    obj->set_vx(0.0f);
    obj->set_vy(0.0f);

    std::vector<uint8_t> buffer(pkt.ByteSizeLong() + 100); // 넉넉하게 할당

    try
    {
        // SerializeToArray 내부에서 float 값을 쓸 때 exception이 발생하는지 확인
        bool result = pkt.SerializeToArray(buffer.data(), (int)buffer.size());
        EXPECT_TRUE(result);
        if (result)
        {
            std::cout << "[PASS] Serialize with NaN/Inf succeeded (No Crash).\n";
        }
    } catch (...)
    {
        FAIL() << "Crash occurred during serialization of NaN/Inf!";
    }
}

// [Test 2] 대량의 객체(1000개) + 음수 ID (Varint Max Size check)
// ByteSizeLong() 계산과 실제 쓰기 크기의 불일치 가능성 점검
TEST(CrashReproduction, BatchPacket_Size_Mismatch)
{
    S_MoveObjectBatch pkt;
    const int TEST_COUNT = 1000;

    // 1000개의 객체 추가
    for (int i = 0; i < TEST_COUNT; ++i)
    {
        auto *obj = pkt.add_moves();
        // [Inject] Negative ID (Varint takes 10 bytes for negative int32 unless sint32 used)
        obj->set_object_id(-1 * (i + 1));
        obj->set_state(Protocol::ObjectState::MOVING);
        obj->set_x((float)i);
        obj->set_y((float)i);
        obj->set_vx(1.0f);
        obj->set_vy(1.0f);
    }

    // 1. 예상 크기 계산
    size_t expectedSize = pkt.ByteSizeLong();

    // 2. [Risk] 딱 맞는 크기로 버퍼 할당 (여유분 없음)
    // 만약 ByteSizeLong()이 실제보다 작게 계산된다면 -> 여기서 Heap Buffer Overflow 발생
    std::vector<uint8_t> buffer(expectedSize);

    try
    {
        // 3. 직렬화 시도
        // SerializeToArray는 bounds checking을 수행하지만,
        // 만약 내부 로직 오류로 오버런이 발생하면 crash가 날 수 있음.
        bool result = pkt.SerializeToArray(buffer.data(), (int)expectedSize);

        EXPECT_TRUE(result) << "Serialization failed with exact buffer size.";
        if (result)
        {
            std::cout << "[PASS] Batch packet serialization succeeded with exact size (" << expectedSize
                      << " bytes).\n";
        }
    } catch (const std::exception &e)
    {
        FAIL() << "Exception during serialization: " << e.what();
    } catch (...)
    {
        FAIL() << "Unknown crash/exception during serialization!";
    }
}

// [Test 3] PacketBase Fix 제거 후 검증 (SerializeBodyTo -> Crash?)
// 템플릿 인스턴스화를 위해 간단한 Mock 정의
struct MockHeader
{
    static constexpr size_t SIZE = 4;
    using IdType = uint16_t;
    uint16_t size;
    uint16_t id;
};

// S_MoveObjectBatch를 상속받거나 래핑하는 Packet 클래스 정의
// (ProtobufPacketBase<Derived, Header, Proto> 사용)
class TestBatchPacket : public System::ProtobufPacketBase<TestBatchPacket, MockHeader, Protocol::S_MoveObjectBatch>
{
public:
    static constexpr uint16_t ID = 9999;
};

TEST(CrashReproduction, PacketBase_SerializeBodyTo_CrashCheck)
{
    TestBatchPacket packet;
    auto &proto = packet.GetProto();

    // 1000개 + 음수 ID -> Varint 최대화
    const int TEST_COUNT = 1000;
    for (int i = 0; i < TEST_COUNT; ++i)
    {
        auto *obj = proto.add_moves();
        obj->set_object_id(-1 * (i + 1));
        obj->set_state(Protocol::ObjectState::MOVING);
        obj->set_x((float)i);
        obj->set_y((float)i);
        obj->set_vx(1.0f);
        obj->set_vy(1.0f);
    }

    // PacketBase::GetTotalSize() -> HeaderSize + Proto.ByteSizeLong()
    size_t totalSize = packet.GetTotalSize();
    size_t bodySize = packet.GetBodySize(); // = proto.ByteSizeLong()

    // 버퍼 할당 (딱 BodySize 만큼의 공간 확보)
    std::vector<uint8_t> buffer(totalSize);

    // SerializeBodyTo 호출 -> 내부에서 _proto.SerializeToArray(exact_size) 실행
    // 여기서 크래시가 나야 함 (만약 Fix가 필요했다면)
    try
    {
        void *bodyPtr = buffer.data() + MockHeader::SIZE;
        packet.SerializeBodyTo(bodyPtr);

        std::cout << "[PASS] SerializeBodyTo succeeded with exact size (" << bodySize << "). Fix unnecessary?\n";
    } catch (...)
    {
        FAIL() << "Crash inside SerializeBodyTo!";
    }
}

// [Disabled] Debug 빌드에서 Protobuf DCHECK가 활성화되어 있어 항상 크래시 발생.
// 원인: 동일 Protobuf 객체를 두 스레드가 동시에 읽기(SerializeToArray) + 쓰기(Clear/add_moves)
//       → Data Race → Protobuf 내부 `Check failed: target + size == res` DCHECK fail.
// Release 빌드(NDEBUG)에서는 DCHECK가 꺼져 있어 조용히 통과됐지만,
// 근본적으로 이 시나리오(객체 공유)는 실제 서버 코드에서 발생하지 않음.
// 실제 서버는 패킷 객체를 스레드 간 공유하지 않으므로 이 테스트는 실용적 의미 없음.
TEST(CrashReproduction, DISABLED_Concurrent_Access_RaceWait)
{
    S_MoveObjectBatch pkt;
    const int ITEM_COUNT = 1000;
    std::atomic<bool> stop{false};

    // 초기 데이터 채우기
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        auto *obj = pkt.add_moves();
        obj->set_object_id(i);
        obj->set_state(Protocol::ObjectState::MOVING);
        obj->set_x((float)i);
        obj->set_y((float)i);
        obj->set_vx(1.0f);
        obj->set_vy(1.0f);
    }

    // Thread 1: 계속 Serialize
    std::thread t1(
        [&]()
        {
            std::vector<uint8_t> buffer(pkt.ByteSizeLong() + 1024);
            while (!stop)
            {
                try
                {
                    // SerializeToArray 호출 (읽기 작업)
                    // 만약 t2가 동시에 쓰면 내부 포인터가 꼬여서 크래시 가능성
                    pkt.SerializeToArray(buffer.data(), (int)buffer.size());
                } catch (...)
                {
                }
            }
        }
    );

    // Thread 2: 계속 Modify (Clear & Add)
    std::thread t2(
        [&]()
        {
            while (!stop)
            {
                pkt.Clear();
                for (int i = 0; i < ITEM_COUNT; ++i)
                {
                    auto *obj = pkt.add_moves();
                    obj->set_object_id(i);
                    obj->set_state(Protocol::ObjectState::MOVING);
                    obj->set_x((float)i);
                    obj->set_y((float)i);
                    obj->set_vx(1.0f);
                    obj->set_vy(1.0f);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 2초간 수행
    stop = true;
    t1.join();
    t2.join();
}

// [Test 5] Integer Truncation (Buffer Overflow)
// [Test 5] Integer Truncation (Buffer Overflow)
// PacketBase.h에서 MaxPacketSize(65535)를 초과하면 assert가 발생해야 함.
TEST(CrashReproduction, Large_Packet_Safety_Check)
{
    TestBatchPacket packet;
    auto &proto = packet.GetProto();

    // 3000개 -> 약 100KB 예상 (개당 30~40바이트)
    const int TEST_COUNT = 3000;
    for (int i = 0; i < TEST_COUNT; ++i)
    {
        auto *obj = proto.add_moves();
        obj->set_object_id(i);
        obj->set_state(Protocol::ObjectState::MOVING);
        obj->set_x(100.0f);
        obj->set_y(200.0f);
        obj->set_vx(10.0f);
        obj->set_vy(20.0f);
        obj->set_look_left(true);
    }

    // 이제는 Assert가 발생해야 정상
    // Google Test의 Death Test 기능을 사용하여 Assert 감지
    EXPECT_DEATH(
        {
            std::vector<uint8_t> buffer(65535);
            packet.SerializeTo(buffer.data());
        },
        "Packet too large"
    );
}
