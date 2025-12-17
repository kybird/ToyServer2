#include "Game/Room.h"
#include "Entity/Player.h"
#include "Entity/PlayerFactory.h"
#include "System/ObjectManager.h"
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
    // Fix: Room ctor needs 3 args
    Room room(1, nullptr, nullptr);
    EXPECT_EQ(room.GetId(), 1);
    EXPECT_EQ(room.GetPlayerCount(), 0);

    // Create a player
    MockSession session1(100);
    // Use Factory
    ObjectManager objMgr; 
    auto player1 = PlayerFactory::Instance().CreatePlayer(objMgr, 1, &session1);

    // Enter
    room.Enter(player1);
    EXPECT_EQ(room.GetPlayerCount(), 1);

    // Leave
    room.Leave(100);
    EXPECT_EQ(room.GetPlayerCount(), 0);
    
    // Release back to factory (room calls leave but doesn't release shared_ptr held here)
    // Custom deleter in factory logic handles release when shared_ptr dies?
    // PlayerFactory creates shared_ptr with custom deleter that calls Release.
    // So when player1 goes out of scope, it should return to pool.
}

TEST(RoomTest, MultiplePlayers)
{
    Room room(2, nullptr, nullptr);
    
    MockSession s1(101);
    MockSession s2(102);
    
    ObjectManager objMgr;
    auto p1 = PlayerFactory::Instance().CreatePlayer(objMgr, 101, &s1);
    auto p2 = PlayerFactory::Instance().CreatePlayer(objMgr, 102, &s2); // Use diff GameIDs

    room.Enter(p1);
    room.Enter(p2);

    EXPECT_EQ(room.GetPlayerCount(), 2);

    room.Leave(101);
    EXPECT_EQ(room.GetPlayerCount(), 1);
    
    room.Leave(102);
    EXPECT_EQ(room.GetPlayerCount(), 0);
}
