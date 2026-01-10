#include "Entity/Player.h"
#include "Game/Room.h"
#include "Game/RoomManager.h"
#include "Protocol/game.pb.h"
#include <gtest/gtest.h>

using namespace SimpleGame;

class MockSession : public System::ISession
{
public:
    MockSession(uint64_t id) : _id(id)
    {
    }
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
    }
    void OnPong() override
    {
    }

private:
    uint64_t _id;
};

class VerificationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup Room
        room = RoomManager::Instance().CreateRoom(999, "TestRoom");
        room->Start();

        // Setup Player
        session = std::make_shared<MockSession>(100);
        player = std::make_shared<Player>(1, session.get());
        player->Initialize(1, session.get());
        room->Enter(player);
    }

    void TearDown() override
    {
        room->Stop();
    }

    std::shared_ptr<Room> room;
    std::shared_ptr<Player> player;
    std::shared_ptr<MockSession> session;
};

// 1. Normal Movement
TEST_F(VerificationTest, NormalMovement_Monotonic)
{
    // Initial Pos: 0,0
    player->SetPos(0, 0);
    player->SetVelocity(10, 0); // Moving Right at 10 unit/s
    player->UpdateLastSentState(0.0f, 0);

    // Simulate 1 tick (0.05s)
    room->Update(0.05f);

    EXPECT_FLOAT_EQ(player->GetX(), 0.5f); // 10 * 0.05
    EXPECT_FLOAT_EQ(player->GetY(), 0.0f);
}

// 2. Direction Flip Stress
TEST_F(VerificationTest, DirectionFlip_Immediate)
{
    // Initial: Moving Right
    player->SetPos(0, 0);
    player->SetVelocity(10, 0);
    room->Update(0.05f);
    EXPECT_FLOAT_EQ(player->GetX(), 0.5f);

    // Flip Left
    player->SetVelocity(-10, 0);
    room->Update(0.05f);

    // Should move back to 0.0
    EXPECT_FLOAT_EQ(player->GetX(), 0.0f);
    EXPECT_EQ(player->GetVX(), -10.0f);
}

// 5. Correction Trigger
TEST_F(VerificationTest, CorrectionTrigger_OnLargeError)
{
    // Initial
    player->SetPos(0, 0);
    player->SetVelocity(0, 0);
    player->UpdateLastSentState(0.0f, 0);

    // Force Large Error (Teleport without dirty)
    // Manually set pos to 100,0 (Huge error vs 0,0)
    player->SetPos(100.0f, 0.0f);

    // Update Room (Should catch error)
    // We can't easily check if packet was sent without observing the broadcast.
    // However, we can check if LastSentState was updated (UpdateLastSentState is called when dirty)
    // LastSentTime should be updated to current time (0.05)

    room->Update(0.05f);

    // If Dirty was triggered, LastSentX should match current X (100)
    EXPECT_FLOAT_EQ(player->GetLastSentX(), 100.0f);
    // And LastSentTime should be 0.05
    // (Note: UpdateLastSentState sets it to _totalRunTime. Room _totalRunTime started at 0, +0.05)
    // Actually Room::Start might not reset totalRunTime? It does in constructor.
    // Update increments it.
}
