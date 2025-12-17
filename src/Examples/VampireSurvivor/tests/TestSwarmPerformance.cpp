#include <gtest/gtest.h>
#include <chrono>
#include "Game/Room.h"
#include "System/ObjectManager.h"
#include "System/SpatialGrid.h"
#include "Entity/MonsterFactory.h"
#include "Entity/Monster.h"
#include "Entity/AI/ChaserAI.h"
#include "System/ITimer.h"


using namespace SimpleGame;

// Mock Timer to manual stepping
class MockTimer : public System::ITimer {
public:
    // Implement the ITimer interface required for Room
    TimerHandle SetTimer(uint32_t timerId, uint32_t delayMs, System::ITimerListener* listener, void* pParam = nullptr) override { return 0; }
    TimerHandle SetTimer(uint32_t timerId, uint32_t delayMs, std::weak_ptr<System::ITimerListener> listener, void* pParam = nullptr) override { return 0; }
    TimerHandle SetInterval(uint32_t timerId, uint32_t intervalMs, System::ITimerListener* listener, void* pParam = nullptr) override { return 0; }
    TimerHandle SetInterval(uint32_t timerId, uint32_t intervalMs, std::weak_ptr<System::ITimerListener> listener, void* pParam = nullptr) override { return 0; }
    void CancelTimer(TimerHandle handle) override {}
    void Unregister(System::ITimerListener* listener) override {}
};

class SwarmPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup Room with Mock Timer
        timer = std::make_shared<MockTimer>();
        room = std::make_shared<Room>(1, timer, nullptr);
    }

    std::shared_ptr<MockTimer> timer;
    std::shared_ptr<Room> room;
};

TEST_F(SwarmPerformanceTest, StressTest500Monsters) {
    // 1. Manually Inject Monsters into Room's components
    const int MONSTER_COUNT = 500;
    
    // Access Private Members (friend)
    auto& objMgr = room->_objMgr;
    auto& grid = room->_grid;

    // Create 500 Monsters
    for(int i=0; i<MONSTER_COUNT; ++i) {
        auto monster = std::make_shared<Monster>(objMgr.GenerateId(), 1);
        monster->SetPos((float)(rand()%100), (float)(rand()%100));
        monster->SetVelocity(1.0f, 1.0f);
        monster->SetAI(std::make_unique<ChaserAI>(2.0f));
        
        objMgr.AddObject(monster);
        grid.Add(monster);
    }

    ASSERT_EQ(objMgr.GetAllObjects().size(), MONSTER_COUNT);

    // 2. Measure Tick Time
    auto start = std::chrono::high_resolution_clock::now();
    const int TICKS = 100;
    
    for(int i=0; i<TICKS; ++i) {
        room->Update(0.05f); // 50ms delta
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    double totalTime = diff.count();
    double avgTickTime = totalTime / TICKS;
    double fps = 1.0 / avgTickTime;
    
    // Log Results
    std::cout << "Processed " << TICKS << " ticks with " << MONSTER_COUNT << " entities." << std::endl;
    std::cout << "Total Time: " << totalTime << "s" << std::endl;
    std::cout << "Avg Tick Time: " << avgTickTime * 1000.0 << "ms" << std::endl;
    std::cout << "Projected Max FPS: " << fps << std::endl;

    // Requirement: 15-20 FPS target means Tick must complete in < 50ms (or 66ms)
    // To be safe, let's say we want to handle this in < 10ms per tick (giving room for overhead)
    // For 500 entities, O(N) or O(N*M) collisions.
    // Our Update is currently O(N) mostly + Grid Update.
    
    EXPECT_LT(avgTickTime, 0.050); // Must be faster than 50ms (20FPS)
}
