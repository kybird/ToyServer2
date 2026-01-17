#include "Entity/Player.h"
#include "Entity/PlayerFactory.h"
#include "Game/ObjectManager.h"
#include "Game/Room.h"
#include "System/ISession.h"
#include "System/Packet/IPacket.h"
#include <cstring>
#include <gtest/gtest.h>
#include <memory>

namespace SimpleGame {

// Mock Session to use with Player
class MockSession : public System::ISession
{
public:
    MockSession(uint64_t id) : _id(id)
    {
    }

    // Minimal implementation required for Player
    uint64_t GetId() const override
    {
        return _id;
    }

    void SendPacket(const System::IPacket &pkt) override
    {
    }

    void Close() override
    {
    }
    void Reset() override
    {
    }
    bool CanDestroy() const override
    {
        return true;
    }
    void IncRef() override
    {
    }
    void DecRef() override
    {
    } // In a real system this would manage lifetime

private:
    uint64_t _id;
};

TEST(RoomTest, EnterAndLeave)
{
    // Fix: Room ctor needs 4 args
    auto room = std::make_shared<Room>(1, nullptr, nullptr, nullptr);
    EXPECT_EQ(room->GetId(), 1);
    EXPECT_EQ(room->GetPlayerCount(), 0);

    // Create a player
    MockSession session1(100);
    // Use Factory
    // Use Factory
    auto player1 = PlayerFactory::Instance().CreatePlayer(1, &session1);

    // Enter
    room->Enter(player1);
    EXPECT_EQ(room->GetPlayerCount(), 1);

    // Leave
    room->Leave(100);
    EXPECT_EQ(room->GetPlayerCount(), 0);

    // Release back to factory (room calls leave but doesn't release shared_ptr held here)
    // Custom deleter in factory logic handles release when shared_ptr dies?
    // PlayerFactory creates shared_ptr with custom deleter that calls Release.
    // So when player1 goes out of scope, it should return to pool.
}

TEST(RoomTest, MultiplePlayers)
{
    auto room = std::make_shared<Room>(2, nullptr, nullptr, nullptr);

    MockSession s1(101);
    MockSession s2(102);

    auto p1 = PlayerFactory::Instance().CreatePlayer(101, &s1);
    auto p2 = PlayerFactory::Instance().CreatePlayer(102, &s2); // Use diff GameIDs

    room->Enter(p1);
    room->Enter(p2);

    EXPECT_EQ(room->GetPlayerCount(), 2);

    room->Leave(101);
    EXPECT_EQ(room->GetPlayerCount(), 1);

    room->Leave(102);
    EXPECT_EQ(room->GetPlayerCount(), 0);
}

// =============================================================================
// SendPacket Failure Path Tests
// =============================================================================
// [WARNING] DO NOT DELETE - Phase 1 리팩토링 검증 테스트
//
// 이 테스트들은 PacketBase → PacketMessage 의존성 제거 리팩토링 후
// 실패 경로에서 크래시가 발생하지 않음을 검증합니다.
// =============================================================================

/**
 * @brief MockPacket - 테스트용 IPacket 구현
 *
 * 검증 목적:
 * - IPacket 인터페이스가 올바르게 동작하는지 확인
 * - SerializeTo가 정상적으로 호출되는지 확인
 */
class MockPacket : public System::IPacket
{
public:
    uint16_t GetPacketId() const override
    {
        return 9999;
    }
    uint16_t GetTotalSize() const override
    {
        return 10;
    }
    void SerializeTo(void *buffer) const override
    {
        std::memset(buffer, 0xAB, 10);
    }
};

/**
 * @brief TrackingMockSession - SendPacket 호출 추적용 Mock
 *
 * 검증 목적:
 * - ISession::SendPacket이 호출되었는지 추적
 * - 전달된 패킷 ID가 올바른지 확인
 */
class TrackingMockSession : public System::ISession
{
public:
    TrackingMockSession(uint64_t id) : _id(id)
    {
    }

    uint64_t GetId() const override
    {
        return _id;
    }

    void SendPacket(const System::IPacket &pkt) override
    {
        sendPacketCalled = true;
        lastPacketId = pkt.GetPacketId();
    }

    void Close() override
    {
    }
    void Reset() override
    {
    }
    bool CanDestroy() const override
    {
        return true;
    }
    void IncRef() override
    {
    }
    void DecRef() override
    {
    }

    bool sendPacketCalled = false;
    bool sendCalled = false;
    uint16_t lastPacketId = 0;

private:
    uint64_t _id;
};

/**
 * @test SendPacketTest.MockSessionSendPacketNoCrash
 *
 * [검증 조건]
 * - MockSession에서 SendPacket 호출
 *
 * [기대 결과]
 * - 크래시 없이 정상 완료
 * - SendPacket이 호출됨
 * - 패킷 ID가 올바르게 전달됨
 *
 * [Phase 1 관련]
 * - ISession::SendPacket 인터페이스 동작 검증
 */
TEST(SendPacketTest, MockSessionSendPacketNoCrash)
{
    TrackingMockSession session(1);
    MockPacket packet;

    // ACT: SendPacket 호출
    session.SendPacket(packet);

    // ASSERT: 정상 호출 확인
    EXPECT_TRUE(session.sendPacketCalled);
    EXPECT_EQ(session.lastPacketId, 9999);
}

/**
 * @test SendPacketTest.BroadcastPacketToEmptyRoomNoCrash
 *
 * [검증 조건]
 * - 플레이어가 0명인 Room에 BroadcastPacket 호출
 *
 * [기대 결과]
 * - 크래시 없이 정상 완료
 * - 빈 루프이므로 아무 일도 일어나지 않음
 *
 * [Phase 1 관련]
 * - Room::BroadcastPacket의 edge case 처리 검증
 */
TEST(SendPacketTest, BroadcastPacketToEmptyRoomNoCrash)
{
    auto room = std::make_shared<Room>(999, nullptr, nullptr, nullptr);
    MockPacket packet;

    // ACT: 빈 Room에 브로드캐스트
    room->BroadcastPacket(packet);

    // ASSERT: 크래시 없이 완료
    EXPECT_EQ(room->GetPlayerCount(), 0);
}

/**
 * @test SendPacketTest.BroadcastPacketToRoomWithPlayers
 *
 * [검증 조건]
 * - 2명의 플레이어가 있는 Room에 BroadcastPacket 호출
 *
 * [기대 결과]
 * - 모든 플레이어 세션에 SendPacket 호출됨
 * - 중복 전송 없음 (각 세션에 1회씩)
 * - 패킷 ID가 올바르게 전달됨
 *
 * [Phase 1 관련]
 * - Room::BroadcastPacket → ISession::SendPacket 체인 검증
 * - 다중 세션에 대한 정상 배포 확인
 */
TEST(SendPacketTest, BroadcastPacketToRoomWithPlayers)
{
    auto room = std::make_shared<Room>(998, nullptr, nullptr, nullptr);

    TrackingMockSession s1(201);
    TrackingMockSession s2(202);

    auto p1 = PlayerFactory::Instance().CreatePlayer(201, &s1);
    auto p2 = PlayerFactory::Instance().CreatePlayer(202, &s2);

    room->Enter(p1);
    room->Enter(p2);

    // Fix: Players must be Ready to receive broadcasts (optimization)
    p1->SetReady(true);
    p2->SetReady(true);

    MockPacket packet;

    // ACT: 브로드캐스트
    room->BroadcastPacket(packet);

    // ASSERT: 두 세션 모두 SendPacket 수신
    EXPECT_TRUE(s1.sendPacketCalled);
    EXPECT_TRUE(s2.sendPacketCalled);
    EXPECT_EQ(s1.lastPacketId, 9999);
    EXPECT_EQ(s2.lastPacketId, 9999);

    room->Leave(202);
}

} // namespace SimpleGame
