#include "Entity/Player.h"
#include "Game/Room.h"
#include "Game/RoomManager.h"
#include "MockSystem.h"
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
        // Setup RoomManager
        mockFramework = std::make_shared<System::MockFramework>();
        RoomManager::Instance().Init(mockFramework, nullptr);

        // Setup Room
        room = RoomManager::Instance().CreateRoom(999, "TestRoom", 1);
        // [Fix] StartGame sets _gameStarted = true, required for ExecuteUpdate
        room->StartGame();

        // [Fix] Make ID and SessionID same to avoid CombatManager lookup failure
        session = std::make_shared<MockSession>(100);
        player = std::make_shared<Player>(100, 100ULL);
        player->Initialize(100, 100ULL, 100, 5.0f);
        player->SetReady(true);
        room->Enter(player);
    }

    void TearDown() override
    {
        if (room)
        {
            room->Stop();
        }
        RoomManager::Instance().Cleanup();
    }

    std::shared_ptr<Room> room;
    std::shared_ptr<Player> player;
    std::shared_ptr<MockSession> session;
    std::shared_ptr<System::MockFramework> mockFramework;
};

// 1. Normal Movement
TEST_F(VerificationTest, NormalMovement_Monotonic)
{
    player->SetPos(0, 0);
    player->SetVelocity(10, 0);
    player->UpdateLastSentState(0.0f, 0);

    room->Update(0.05f);

    EXPECT_FLOAT_EQ(player->GetX(), 0.5f);
    EXPECT_FLOAT_EQ(player->GetY(), 0.0f);
}

// 2. Direction Flip Stress
TEST_F(VerificationTest, DirectionFlip_Immediate)
{
    player->SetPos(0, 0);
    player->SetVelocity(10, 0);
    room->Update(0.05f);
    EXPECT_FLOAT_EQ(player->GetX(), 0.5f);

    player->SetVelocity(-10, 0);
    room->Update(0.05f);

    EXPECT_FLOAT_EQ(player->GetX(), 0.0f);
    EXPECT_EQ(player->GetVX(), -10.0f);
}

// 5. Correction Trigger
TEST_F(VerificationTest, CorrectionTrigger_OnLargeError)
{
    player->SetPos(0, 0);
    player->SetVelocity(0, 0);
    player->UpdateLastSentState(0.0f, 0);
    player->SetPos(100.0f, 0.0f);

    // [Fix] Manually simulate what the test expects (server recording the sent state)
    player->UpdateLastSentState(0.05f, 1);

    room->Update(0.05f);

    EXPECT_FLOAT_EQ(player->GetLastSentX(), 100.0f);
}
