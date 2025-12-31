#include <gtest/gtest.h>
#include <chrono>
#include "Game/Room.h"
#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"
#include "Entity/MonsterFactory.h"
#include "Entity/Monster.h"
#include "Entity/AI/ChaserAI.h"
#include "System/ITimer.h"
#include <iostream>

namespace SimpleGame {

// Mock Timer to manual stepping
class MockTimer : public System::ITimer {
public:
    TimerHandle SetTimer(uint32_t timerId, uint32_t delayMs, System::ITimerListener* listener, void* pParam = nullptr) override { return 0; }
    TimerHandle SetTimer(uint32_t timerId, uint32_t delayMs, std::weak_ptr<System::ITimerListener> listener, void* pParam = nullptr) override { return 0; }
    TimerHandle SetInterval(uint32_t timerId, uint32_t intervalMs, System::ITimerListener* listener, void* pParam = nullptr) override { return 0; }
    TimerHandle SetInterval(uint32_t timerId, uint32_t intervalMs, std::weak_ptr<System::ITimerListener> listener, void* pParam = nullptr) override { return 0; }
    void CancelTimer(TimerHandle handle) override {}
    void Unregister(System::ITimerListener* listener) override {}
};

TEST(SwarmPerformanceTest, StressTest500Monsters) {
    // Initialize DataManager with mock template
    MonsterTemplate tmpl;
    tmpl.id = 1;
    tmpl.hp = 100;
    tmpl.speed = 2.0f;
    tmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterTemplate(tmpl);

    auto timer = std::make_shared<MockTimer>();
    auto room = std::make_shared<Room>(1, timer, nullptr, nullptr);

    const int MONSTER_COUNT = 500;
    
    // Create 500 Monsters via MonsterFactory and Room methods if possible, 
    // or just use public interfaces.
    // Since we are friends, we can use _objMgr directly but let's be careful with ptr type.
    
    for(int i=0; i<MONSTER_COUNT; ++i) {
        auto monster = MonsterFactory::Instance().CreateMonster(room->_objMgr, 1, (float)(rand()%100), (float)(rand()%100));
        if (monster) {
            monster->SetVelocity(1.0f, 1.0f);
            room->_objMgr.AddObject(monster);
            room->_grid.Add(monster);
        }
    }

    // 2. Measure Tick Time
    auto start = std::chrono::high_resolution_clock::now();
    const int TICKS = 100;
    
    for(int i=0; i<TICKS; ++i) {
        room->Update(0.05f); 
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    double totalTime = diff.count();
    double avgTickTime = totalTime / TICKS;
    
    std::cout << "Processed " << TICKS << " ticks with " << MONSTER_COUNT << " entities." << std::endl;
    std::cout << "Avg Tick Time: " << avgTickTime * 1000.0 << "ms" << std::endl;
    
    EXPECT_LT(avgTickTime, 0.050); 
}

}
