#include "Room.h"
#include "Player.h"
#include <gtest/gtest.h>
#include <memory>

using namespace SimpleGame;

// Mock Session to use with Player
class MockSession : public System::ISession
{
public:
    MockSession(uint64_t id) : _id(id) {}
    
    // Minimal implementation required for Player
    uint64_t GetId() const override { return _id; }
    
    void Send(System::PacketMessage* msg) override {}
    void Send(std::span<const uint8_t> data) override {}
    
    void Close() override {}
    void Reset() override {}
    bool CanDestroy() const override { return true; }
    void DecRef() override {} // In a real system this would manage lifetime

private:
    uint64_t _id;
};

TEST(RoomTest, EnterAndLeave)
{
    Room room(1);
    EXPECT_EQ(room.GetId(), 1);
    EXPECT_EQ(room.GetPlayerCount(), 0);

    // Create a player
    MockSession session1(100);
    auto player1 = std::make_shared<Player>(&session1);

    // Enter
    room.Enter(player1);
    EXPECT_EQ(room.GetPlayerCount(), 1);

    // Leave
    room.Leave(100);
    EXPECT_EQ(room.GetPlayerCount(), 0);
}

TEST(RoomTest, MultiplePlayers)
{
    Room room(2);
    
    MockSession s1(101);
    MockSession s2(102);
    
    auto p1 = std::make_shared<Player>(&s1);
    auto p2 = std::make_shared<Player>(&s2);

    room.Enter(p1);
    room.Enter(p2);

    EXPECT_EQ(room.GetPlayerCount(), 2);

    room.Leave(101);
    EXPECT_EQ(room.GetPlayerCount(), 1);
    
    room.Leave(102);
    EXPECT_EQ(room.GetPlayerCount(), 0);
}
