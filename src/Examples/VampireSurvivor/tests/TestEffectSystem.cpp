#include "Entity/AI/IAIBehavior.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Game/Effect/EffectManager.h"
#include "Game/Room.h"
#include "Protocol/game.pb.h"
#include <gtest/gtest.h>


using namespace SimpleGame;

class MockAI : public IAIBehavior
{
public:
    bool executed = false;
    void Think(Monster *monster, Room *room, float currentTime) override
    {
    }
    void Execute(Monster *monster, float dt) override
    {
        executed = true;
    }
    void Reset() override
    {
        executed = false;
    }
};

TEST(EffectSystemTest, KnockbackStateBlocksAI)
{
    auto monster = std::make_shared<Monster>(1, 101);
    auto mockAI = std::make_unique<MockAI>();
    auto *aiPtr = mockAI.get();
    monster->SetAI(std::move(mockAI));

    // 1. Normal state: AI should execute
    monster->Update(0.1f, nullptr);
    EXPECT_TRUE(aiPtr->executed);
    aiPtr->executed = false;

    // 2. Set Knockback state
    monster->SetState(Protocol::ObjectState::KNOCKBACK, 1.0f); // Expires at 1.0s
    monster->Update(0.1f, nullptr);
    EXPECT_FALSE(aiPtr->executed); // AI should be blocked

    // 3. Wait for expiry
    monster->Update(1.0f, nullptr); // Current time 1.2s > expiry 1.0s
    EXPECT_EQ(monster->GetState(), Protocol::ObjectState::IDLE);

    monster->Update(0.1f, nullptr);
    EXPECT_TRUE(aiPtr->executed); // AI should resume
}

TEST(EffectSystemTest, SlowEffectIntegration)
{
    EffectManager mgr;
    int32_t targetId = 100;

    // Apply Slow 50% for 1 second
    Effect::StatusEffect slow;
    slow.type = Effect::Type::SLOW;
    slow.value = 0.5f;
    slow.endTime = 1.0f;
    mgr.ApplyEffect(targetId, slow);

    EXPECT_FLOAT_EQ(mgr.GetSpeedMultiplier(targetId), 0.5f);

    // After 1 sec
    mgr.Update(1.1f, nullptr);
    EXPECT_FLOAT_EQ(mgr.GetSpeedMultiplier(targetId), 1.0f);
}
