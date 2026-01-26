#include "Core/DataManager.h"
#include "Entity/Monster.h"
#include "Entity/MonsterFactory.h"
#include "Entity/Player.h"
#include "Game/ObjectManager.h"
#include "Game/Room.h"
#include <chrono>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>

using namespace std::chrono;

namespace SimpleGame {

class SwarmPerformanceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Ensure data/MonsterData.json exists or create dummy
        std::ifstream f("data/MonsterData.json");
        if (!f.good())
        {
            std::ofstream out("data/MonsterData.json");
            out << "[{\"id\":1, \"hp\":100, \"speed\":2.0, \"aiType\":0}]"; // 0 = Chaser
            out.close();
        }

        // Load Data
        if (!DataManager::Instance().LoadMonsterData("data/MonsterData.json"))
        {
            // If failed, manual load dummy?
        }
    }
};

TEST_F(SwarmPerformanceTest, StressTest500Monsters)
{
    auto room = std::make_shared<Room>(1, nullptr, nullptr, nullptr, nullptr);

    // Access private members via FRIEND class
    ObjectManager &objMgr = room->_objMgr;
    SpatialGrid &grid = room->_grid;

    // Spawn 500 Monsters
    int count = 500;
    auto monsters = MonsterFactory::Instance().SpawnBatch(objMgr, 1, count, 0, 2000, 0, 2000);

    ASSERT_EQ(monsters.size(), count);

    // Add to Grid manually (Simulating WaveManager)
    for (auto &m : monsters)
    {
        grid.Add(m);
    }

    // Add Dummy Player to trigger AI
    auto player = std::make_shared<Player>(1000, 0ULL);
    player->Initialize(1000, 0ULL, 100, 5.0f);
    player->SetPos(1000.0f, 1000.0f); // Center
    room->Enter(player);

    // Run Loop
    std::cout << "Starting 500 Monster Update Loop (1000 iterations)..." << std::endl;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        // Mock DeltaTime 33ms
        room->Update(0.033f);
    }

    auto end = high_resolution_clock::now();
    long long duration = duration_cast<milliseconds>(end - start).count();

    std::cout << "Total Time: " << duration << " ms" << std::endl;
    std::cout << "Average per tick: " << (double)duration / 1000.0 << " ms" << std::endl;

    // Assert Performance expectation
    EXPECT_LT(duration, 33000);
}

} // namespace SimpleGame
