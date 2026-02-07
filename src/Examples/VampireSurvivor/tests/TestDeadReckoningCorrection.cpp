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
        // Setup Room
        room = RoomManager::Instance().CreateRoom(999, "TestRoom");
        room->Start();

        // Setup Player
        session = std::make_shared<MockSession>(100);
        player = std::make_shared<Player>(1, session->GetId());
        player->Initialize(1, session->GetId(), 100, 5.0f);
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

    room->Update(0.05f);

    EXPECT_FLOAT_EQ(player->GetLastSentX(), 100.0f);
}
