#include "Entity/Player.h"
#include "Game/ObjectManager.h"
#include "Game/Room.h"
#include "System/ISession.h"
#include "System/MockSystem.h"
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
    void SendPacket(System::PacketPtr msg) override
    {
    }
    void SendPreSerialized(const System::PacketMessage *msg) override
    {
    }
    void SendReliable(const System::IPacket &pkt) override
    {
    }
    void SendUnreliable(const System::IPacket &pkt) override
    {
    }
    void OnConnect() override
    {
    }
    void OnDisconnect() override
    {
    }
    bool IsConnected() const override
    {
        return true;
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

private:
    uint64_t _id;
};

TEST(RoomTest, EnterAndLeave)
{
    // Room 생성자 인자 수정 (6개)
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    EXPECT_EQ(room->GetId(), 1);
    EXPECT_EQ(room->GetPlayerCount(), 0);

    // Create a player
    MockSession session1(100);
    auto player1 = std::make_shared<Player>(1, session1.GetId());
    player1->Initialize(1, session1.GetId(), 100, 5.0f);

    // Enter
    room->Enter(player1);
    EXPECT_EQ(room->GetPlayerCount(), 1);

    // Leave
    room->Leave(100);
    EXPECT_EQ(room->GetPlayerCount(), 0);
}

TEST(RoomTest, MultiplePlayers)
{
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        2,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );

    MockSession s1(101);
    MockSession s2(102);

    auto p1 = std::make_shared<Player>(101, s1.GetId());
    p1->Initialize(101, s1.GetId(), 100, 5.0f);
    auto p2 = std::make_shared<Player>(102, s2.GetId());
    p2->Initialize(102, s2.GetId(), 100, 5.0f);

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

/**
 * @brief MockPacket - 테스트용 IPacket 구현
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
    void SendPacket(System::PacketPtr msg) override
    {
        sendPacketCalled = true;
        // In the test context, we know we are broadcasting ID 9999
        lastPacketId = 9999;
    }
    void SendPreSerialized(const System::PacketMessage *msg) override
    {
    }
    void SendReliable(const System::IPacket &pkt) override
    {
        sendPacketCalled = true;
        lastPacketId = pkt.GetPacketId();
    }
    void SendUnreliable(const System::IPacket &pkt) override
    {
        sendPacketCalled = true;
        lastPacketId = pkt.GetPacketId();
    }
    void OnConnect() override
    {
    }
    void OnDisconnect() override
    {
    }
    bool IsConnected() const override
    {
        return true;
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
    uint16_t lastPacketId = 0;

private:
    uint64_t _id;
};

TEST(SendPacketTest, MockSessionSendPacketNoCrash)
{
    TrackingMockSession session(1);
    MockPacket packet;

    session.SendPacket(packet);

    EXPECT_TRUE(session.sendPacketCalled);
    EXPECT_EQ(session.lastPacketId, 9999);
}

TEST(SendPacketTest, BroadcastPacketToEmptyRoomNoCrash)
{
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        999,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    MockPacket packet;

    room->BroadcastPacket(packet);

    EXPECT_EQ(room->GetPlayerCount(), 0);
}

TEST(SendPacketTest, BroadcastPacketToRoomWithPlayers)
{
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        998,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );

    TrackingMockSession s1(201);
    TrackingMockSession s2(202);

    mockFramework->GetMockDispatcher()->RegisterSession(201, &s1);
    mockFramework->GetMockDispatcher()->RegisterSession(202, &s2);

    auto p1 = std::make_shared<Player>(201, s1.GetId());
    p1->Initialize(201, s1.GetId(), 100, 5.0f);
    auto p2 = std::make_shared<Player>(202, s2.GetId());
    p2->Initialize(202, s2.GetId(), 100, 5.0f);

    room->Enter(p1);
    room->Enter(p2);

    p1->SetReady(true);
    p2->SetReady(true);

    // [Fix] Ensure game is started
    room->StartGame();

    MockPacket packet;
    room->BroadcastPacket(packet);

    // Verify dispatcher was called for both sessions
    auto mockDisp = mockFramework->GetMockDispatcher();
    EXPECT_EQ(mockDisp->calls.size(), 2);

    bool calledS1 = false;
    bool calledS2 = false;
    for (const auto &call : mockDisp->calls)
    {
        if (call.sessionId == 201)
            calledS1 = true;
        if (call.sessionId == 202)
            calledS2 = true;
    }
    EXPECT_TRUE(calledS1);
    EXPECT_TRUE(calledS2);

    room->Leave(202);
}

} // namespace SimpleGame
