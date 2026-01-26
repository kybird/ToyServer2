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
    void SendPacket(System::PacketPtr msg) override
    {
    }
    void SendPreSerialized(const System::PacketMessage *msg) override
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
    // Fix: Room ctor needs 5 args
    auto room = std::make_shared<Room>(1, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(room->GetId(), 1);
    EXPECT_EQ(room->GetPlayerCount(), 0);

    // Create a player
    MockSession session1(100);
    // Use Factory
    auto player1 = PlayerFactory::Instance().CreatePlayer(1, session1.GetId());

    // Enter
    room->Enter(player1);
    EXPECT_EQ(room->GetPlayerCount(), 1);

    // Leave
    room->Leave(100);
    EXPECT_EQ(room->GetPlayerCount(), 0);
}

TEST(RoomTest, MultiplePlayers)
{
    auto room = std::make_shared<Room>(2, nullptr, nullptr, nullptr, nullptr);

    MockSession s1(101);
    MockSession s2(102);

    auto p1 = PlayerFactory::Instance().CreatePlayer(101, s1.GetId());
    auto p2 = PlayerFactory::Instance().CreatePlayer(102, s2.GetId());

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
    }
    void SendPreSerialized(const System::PacketMessage *msg) override
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
    auto room = std::make_shared<Room>(999, nullptr, nullptr, nullptr, nullptr);
    MockPacket packet;

    room->BroadcastPacket(packet);

    EXPECT_EQ(room->GetPlayerCount(), 0);
}

TEST(SendPacketTest, BroadcastPacketToRoomWithPlayers)
{
    auto room = std::make_shared<Room>(998, nullptr, nullptr, nullptr, nullptr);

    TrackingMockSession s1(201);
    TrackingMockSession s2(202);

    auto p1 = PlayerFactory::Instance().CreatePlayer(201, s1.GetId());
    auto p2 = PlayerFactory::Instance().CreatePlayer(202, s2.GetId());

    room->Enter(p1);
    room->Enter(p2);

    p1->SetReady(true);
    p2->SetReady(true);

    MockPacket packet;
    room->BroadcastPacket(packet);

    EXPECT_TRUE(s1.sendPacketCalled);
    EXPECT_TRUE(s2.sendPacketCalled);
    EXPECT_EQ(s1.lastPacketId, 9999);
    EXPECT_EQ(s2.lastPacketId, 9999);

    room->Leave(202);
}

} // namespace SimpleGame
