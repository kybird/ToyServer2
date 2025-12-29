#include "Entity/MonsterFactory.h"
#include "Entity/ProjectileFactory.h"
#include "Game/Room.h"
#include "Entity/Monster.h"
#include "Entity/Projectile.h"
#include "Core/DataManager.h"
#include <gtest/gtest.h>

namespace SimpleGame {

TEST(CombatTest, ProjectileHitsMonster)
{
    // Initialize DataManager with mock template
    MonsterTemplate tmpl;
    tmpl.id = 1;
    tmpl.hp = 100;
    tmpl.speed = 2.0f;
    tmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterTemplate(tmpl);

    Room room(1, nullptr, nullptr, nullptr);
    
    // 1. Create a Monster at (2, 0)
    auto monster = MonsterFactory::Instance().CreateMonster(room._objMgr, 1, 2.0f, 0.0f);
    room._objMgr.AddObject(monster);
    room._grid.Add(monster);
    int32_t initialHp = monster->GetHp();

    // 2. Create a Projectile at (0, 0) moving towards (2, 0)
    auto proj = ProjectileFactory::Instance().CreateProjectile(room._objMgr, 999, 1, 0.0f, 0.0f, 20.0f, 0.0f);
    proj->SetDamage(50);
    room._objMgr.AddObject(proj);
    room._grid.Add(proj);

    // 3. Update Room (dt = 0.1s)
    // In 0.1s, projectile moves 2m -> exactly at (2,0)
    room.Update(0.1f); 

    // 4. Verify Hit
    // Monster should have taken 50 damage
    EXPECT_EQ(monster->GetHp(), initialHp - 50);
    
    // Projectile should be removed (IsExpired because of SetHit)
    EXPECT_EQ(room._objMgr.GetObject(proj->GetId()), nullptr);
}

TEST(CombatTest, MonsterDies)
{
    // Use existing template from previous test or ensure it's there
    MonsterTemplate tmpl;
    tmpl.id = 1;
    tmpl.hp = 100;
    tmpl.speed = 2.0f;
    tmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterTemplate(tmpl);

    Room room(2, nullptr, nullptr, nullptr);
    
    auto monster = MonsterFactory::Instance().CreateMonster(room._objMgr, 1, 0.0f, 0.0f);
    monster->SetHp(10);
    room._objMgr.AddObject(monster);
    room._grid.Add(monster);

    // Overlapping projectile
    auto proj = ProjectileFactory::Instance().CreateProjectile(room._objMgr, 999, 1, 0.1f, 0.1f, 0.0f, 0.0f);
    proj->SetDamage(20);
    room._objMgr.AddObject(proj);
    room._grid.Add(proj);

    room.Update(0.05f);

    // Monster should be dead and removed
    EXPECT_EQ(room._objMgr.GetObject(monster->GetId()), nullptr);
}

} // namespace SimpleGame
