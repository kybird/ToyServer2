#include "Entity/ModifierContainer.h"
#include <gtest/gtest.h>

using namespace SimpleGame;

class StatModifierTest : public ::testing::Test
{
protected:
    ModifierContainer container;

    void SetUp() override
    {
        container.Clear();
        container.SetBaseStat(StatType::Speed, 10.0f);
    }
};

TEST_F(StatModifierTest, BaseStatParams)
{
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 10.0f);
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Attack), 0.0f); // Default 0
}

TEST_F(StatModifierTest, FlatModifier)
{
    // 10 + 5 = 15
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::Flat, 5.0f, 1));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 15.0f);

    // 10 + 5 - 2 = 13
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::Flat, -2.0f, 2));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 13.0f);
}

TEST_F(StatModifierTest, PercentAddModifier)
{
    // 10 * (1 + 0.1) = 11
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::PercentAdd, 0.1f, 1));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 11.0f);

    // 10 * (1 + 0.1 + 0.2) = 13
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::PercentAdd, 0.2f, 2));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 13.0f);
}

TEST_F(StatModifierTest, PercentMultModifier)
{
    // 10 * 0.5 = 5
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::PercentMult, 0.5f, 1));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 5.0f);

    // 10 * 0.5 * 0.5 = 2.5
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::PercentMult, 0.5f, 2));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 2.5f);
}

TEST_F(StatModifierTest, CombinedCalculation)
{
    // Formula: (Base + Flat) * (1 + Sum%Add) * Prod%Mult
    // Base 10
    // Flat +5  => 15
    // %Add 0.2 => 15 * 1.2 = 18
    // %Mult 0.5 => 18 * 0.5 = 9

    container.SetBaseStat(StatType::Speed, 10.0f);
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::Flat, 5.0f, 1));
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::PercentAdd, 0.2f, 2));
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::PercentMult, 0.5f, 3));

    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 9.0f);
}

TEST_F(StatModifierTest, RefreshPolicy)
{
    // Source 1, Value 5.0 (No Stack)
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::Flat, 5.0f, 1, 0.0f, false));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 15.0f);

    // Same Source 1, New Value 10.0 -> Should replace
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::Flat, 10.0f, 1, 0.0f, false));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 20.0f);
}

TEST_F(StatModifierTest, StackPolicy)
{
    // Source 1, Value 5.0 (Allow Stack)
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::Flat, 5.0f, 1, 0.0f, true));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 15.0f);

    // Same Source 1, Value 5.0 -> Should Add
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::Flat, 5.0f, 1, 0.0f, true));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 20.0f);
}

TEST_F(StatModifierTest, Expiration)
{
    // Base 10 + 5 (Expires at 100)
    container.AddModifier(StatModifier(StatType::Speed, ModifierOp::Flat, 5.0f, 1, 100.0f));
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 15.0f);

    // Time 99 -> Still active
    container.Update(99.0f);
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 15.0f);

    // Time 100 -> Expired & Removed
    container.Update(100.0f);
    EXPECT_FLOAT_EQ(container.GetStat(StatType::Speed), 10.0f);
}
