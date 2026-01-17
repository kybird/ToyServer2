#include "Core/DataManager.h"
#include "Entity/Monster.h"
#include "Entity/MonsterFactory.h"
#include "Entity/Player.h"
#include "Entity/PlayerFactory.h"
#include "Entity/Projectile.h"
#include "Entity/ProjectileFactory.h"
#include "Game/DamageEmitter.h"
#include "Game/Room.h"
#include <gtest/gtest.h>

namespace SimpleGame {
// Dummy comment to force recompile

TEST(CombatTest, ProjectileHitsMonster)
{
    // Initialize DataManager with mock template
    MonsterTemplate tmpl;
    tmpl.id = 1;
    tmpl.hp = 100;
    tmpl.speed = 2.0f;
    tmpl.radius = 0.5f;
    tmpl.damageOnContact = 10;
    tmpl.attackCooldown = 1.0f;
    tmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterTemplate(tmpl);

    Room room(1, nullptr, nullptr, nullptr);

    // 1. Create a Monster at (2, 0)
    auto monster = MonsterFactory::Instance().CreateMonster(room._objMgr, 1, 2.0f, 0.0f);
    room._objMgr.AddObject(monster);
    room._grid.Add(monster);
    int32_t initialHp = monster->GetHp();

    // 2. Create a Projectile at (0, 0) moving towards (2, 0)
    auto proj =
        ProjectileFactory::Instance().CreateProjectile(room._objMgr, 999, 1, 1, 0.0f, 0.0f, 20.0f, 0.0f, 50, 2.0f);
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
    MonsterTemplate tmpl;
    tmpl.id = 1;
    tmpl.hp = 100;
    tmpl.speed = 2.0f;
    tmpl.radius = 0.5f;
    tmpl.damageOnContact = 10;
    tmpl.attackCooldown = 1.0f;
    tmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterTemplate(tmpl);

    Room room(2, nullptr, nullptr, nullptr);

    auto monster = MonsterFactory::Instance().CreateMonster(room._objMgr, 1, 0.0f, 0.0f);
    monster->SetHp(10);
    room._objMgr.AddObject(monster);
    room._grid.Add(monster);

    // Overlapping projectile
    auto proj =
        ProjectileFactory::Instance().CreateProjectile(room._objMgr, 999, 1, 1, 0.1f, 0.1f, 0.0f, 0.0f, 20, 2.0f);
    proj->SetDamage(20);
    room._objMgr.AddObject(proj);
    room._grid.Add(proj);

    room.Update(0.05f);

    // Monster should be dead and removed
    EXPECT_EQ(room._objMgr.GetObject(monster->GetId()), nullptr);
}

TEST(CombatTest, MonsterContactsPlayer)
{
    // Setup Templates
    MonsterTemplate mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 100;
    mTmpl.speed = 2.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 20;
    mTmpl.attackCooldown = 1.0f;
    mTmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterTemplate(mTmpl);

    PlayerTemplate pTmpl;
    pTmpl.id = 1;
    pTmpl.hp = 100;
    pTmpl.speed = 5.0f;
    DataManager::Instance().AddPlayerTemplate(pTmpl);

    Room room(3, nullptr, nullptr, nullptr);

    // Create Player at (0, 0)
    auto player = PlayerFactory::Instance().CreatePlayer(100, nullptr);
    player->SetPos(0.0f, 0.0f);
    room._players[100] = player;
    room._objMgr.AddObject(player);
    room._grid.Add(player);

    // Create Monster at (0.5, 0) - Overlapping (sum radius = 1.0)
    auto monster = MonsterFactory::Instance().CreateMonster(room._objMgr, 1, 0.5f, 0.0f);
    room._objMgr.AddObject(monster);
    room._grid.Add(monster);

    // Initial check
    EXPECT_EQ(player->GetHp(), 100);

    // Update Room
    room.Update(0.1f);

    // Player should have taken 20 damage
    EXPECT_EQ(player->GetHp(), 80);

    // Monster should be on cooldown
    EXPECT_FALSE(monster->CanAttack(room._totalRunTime));
}

TEST(CombatTest, OverkillDoesNotResultInNegativeHp)
{
    auto room = std::make_shared<Room>(4, nullptr, nullptr, nullptr);

    // 1. Setup Player with 10 HP
    auto player = std::make_shared<Player>(100, nullptr);
    player->Initialize(100, nullptr, 100, 5.0f);
    player->SetHp(10);
    player->SetHp(10);
    player->SetReady(true); // Fix: Must be ready to update
    room->Enter(player);

    // 2. Apply 100 Damage (Overkill)
    player->TakeDamage(100, room.get());

    // 3. Verify
    EXPECT_EQ(player->GetHp(), 0);
    EXPECT_TRUE(player->IsDead());

    // 4. Setup Monster with 10 HP
    auto monster = std::make_shared<Monster>(200, 1);
    monster->Initialize(200, 1, 100, 0.5f, 10, 1.0f);
    monster->SetHp(10);

    // 5. Apply 100 Damage (Overkill)
    monster->TakeDamage(100, room.get());

    // 6. Verify
    EXPECT_EQ(monster->GetHp(), 0);
    EXPECT_TRUE(monster->IsDead());
}

TEST(CombatTest, LinearEmitterHitsNearestMonster)
{
    // Setup Data
    SkillTemplate sTmpl;
    sTmpl.id = 1;
    sTmpl.name = "base_linear";
    sTmpl.damage = 10;
    sTmpl.tickInterval = 0.5f;
    sTmpl.hitRadius = 2.0f;
    sTmpl.lifeTime = 0.0f;
    sTmpl.emitterType = "Linear";
    sTmpl.maxTargetsPerTick = 1;
    sTmpl.targetRule = "Nearest";
    DataManager::Instance().AddSkillTemplate(sTmpl);

    MonsterTemplate mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 100;
    mTmpl.speed = 0.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 0;
    mTmpl.attackCooldown = 1.0f;
    DataManager::Instance().AddMonsterTemplate(mTmpl);

    auto room = std::make_shared<Room>(5, nullptr, nullptr, nullptr);

    // Player at (0,0) facing right (1,0)
    auto player = std::make_shared<Player>(100, nullptr);
    player->Initialize(100, nullptr, 100, 5.0f);
    player->ApplyInput(1, 1, 0); // Faces (1,0)
    player->SetVelocity(0, 0);   // Stop movement for testing static position
    player->SetReady(true);      // Fix: Must be ready
    room->Enter(player);

    // Add emitter manually (since Enter without DB doesn't add it)
    player->AddEmitter(std::make_shared<DamageEmitter>(1, player));

    // M1 at (1,0) - Nearest
    auto m1 = MonsterFactory::Instance().CreateMonster(room->_objMgr, 1, 1.0f, 0.0f);
    room->_objMgr.AddObject(m1);
    room->_grid.Add(m1);

    // M2 at (1.5,0) - Further
    auto m2 = MonsterFactory::Instance().CreateMonster(room->_objMgr, 1, 1.5f, 0.0f);
    room->_objMgr.AddObject(m2);
    room->_grid.Add(m2);

    // Update (tickInterval elapsed)
    room->Update(0.6f);

    // Verify: M1 hit (100 -> 90), M2 not hit (100)
    EXPECT_EQ(m1->GetHp(), 90);
    EXPECT_EQ(m2->GetHp(), 100);
}

TEST(CombatTest, LinearEmitterRespectsLifetime)
{
    SkillTemplate sTmpl;
    sTmpl.id = 2; // Unique ID for this test
    sTmpl.name = "timed_linear";
    sTmpl.damage = 10;
    sTmpl.tickInterval = 1.0f;
    sTmpl.hitRadius = 2.0f;
    sTmpl.lifeTime = 0.5f; // Short lifetime
    sTmpl.emitterType = "Linear";
    sTmpl.maxTargetsPerTick = 1;
    sTmpl.targetRule = "Nearest";
    DataManager::Instance().AddSkillTemplate(sTmpl);

    auto room = std::make_shared<Room>(6, nullptr, nullptr, nullptr);
    auto player = std::make_shared<Player>(100, nullptr);
    player->Initialize(100, nullptr, 100, 5.0f);
    player->Initialize(100, nullptr, 100, 5.0f);
    player->SetReady(true); // Fix: Must be ready
    room->Enter(player);

    // Add emitter with ID 2
    player->ClearEmitters(); // Clear default emitter from Enter
    player->AddEmitter(std::make_shared<DamageEmitter>(2, player));

    EXPECT_EQ(player->GetEmitterCount(), 1);

    // Update within lifetime
    room->Update(0.3f);
    EXPECT_EQ(player->GetEmitterCount(), 1);

    // past lifetime
    room->Update(0.3f);
    EXPECT_EQ(player->GetEmitterCount(), 0);
}

TEST(CombatTest, DISABLED_MonsterKnockback)
{
    // Feature removed per design decision
}
/*
TEST(CombatTest, MonsterKnockback)
{
    MonsterTemplate mTmpl;
    mTmpl.id = 50; // New ID
    mTmpl.hp = 100;
    mTmpl.speed = 2.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 20;
    mTmpl.attackCooldown = 1.0f;
    mTmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterTemplate(mTmpl);

    PlayerTemplate pTmpl;
    pTmpl.id = 1;
    pTmpl.hp = 100;
    pTmpl.speed = 5.0f;
    DataManager::Instance().AddPlayerTemplate(pTmpl);

    Room room(7, nullptr, nullptr, nullptr);

    auto player = PlayerFactory::Instance().CreatePlayer(100, nullptr);
    player->SetPos(0.0f, 0.0f);
    room._players[100] = player;
    room._objMgr.AddObject(player);
    room._grid.Add(player);

    // Monster overlapping player at (0.1, 0)
    auto monster = MonsterFactory::Instance().CreateMonster(room._objMgr, 50, 0.1f, 0.0f);
    room._objMgr.AddObject(monster);
    room._grid.Add(monster);

    // Update Room
    room.Update(0.01f);

    // Monster should be knocked back in positive X direction (dx = 0.1, player at 0.0)
    EXPECT_GT(monster->GetVX(), 10.0f);
}
*/

TEST(CombatTest, LinearEmitterSpawnsProjectile)
{
    // 1. Setup Data
    SkillTemplate sTmpl;
    sTmpl.id = 10;
    sTmpl.name = "spawn_linear";
    sTmpl.damage = 10;
    sTmpl.tickInterval = 0.5f;
    sTmpl.hitRadius = 2.0f;
    sTmpl.lifeTime = 0.0f;
    sTmpl.typeId = 777; // Expected TypeId
    sTmpl.emitterType = "Linear";
    sTmpl.maxTargetsPerTick = 1;
    sTmpl.targetRule = "Nearest";
    DataManager::Instance().AddSkillTemplate(sTmpl);

    auto room = std::make_shared<Room>(10, nullptr, nullptr, nullptr);

    // 2. Setup Player
    auto player = std::make_shared<Player>(100, nullptr);
    player->Initialize(100, nullptr, 100, 5.0f);
    player->ApplyInput(1, 1, 0); // Faces (1,0)
    player->SetVelocity(0, 0);
    player->ApplyInput(1, 1, 0); // Faces (1,0)
    player->SetVelocity(0, 0);
    player->SetReady(true); // Fix: Must be ready
    room->Enter(player);

    // Add emitter manually
    auto emitter = std::make_shared<DamageEmitter>(10, player);
    player->AddEmitter(emitter);

    // 3. Update Room (tickInterval elapsed)
    room->Update(0.6f);

    // 4. Verify: Projectile should have been spawned
    auto allObjects = room->_objMgr.GetAllObjects();
    bool foundProjectile = false;
    for (const auto &obj : allObjects)
    {
        if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            auto proj = std::dynamic_pointer_cast<Projectile>(obj);
            if (proj && proj->GetSkillId() == 10)
            {
                foundProjectile = true;
                EXPECT_EQ(proj->GetTypeId(), 777);
                EXPECT_EQ(proj->GetOwnerId(), 100);
                EXPECT_GT(std::abs(proj->GetVX()), 0.0f); // Should be moving
                break;
            }
        }
    }
    EXPECT_TRUE(foundProjectile);
}

} // namespace SimpleGame
