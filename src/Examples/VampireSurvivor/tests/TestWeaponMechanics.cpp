#include "Core/DataManager.h"
#include "Entity/Monster.h"
#include "Entity/MonsterFactory.h"
#include "Entity/Player.h"
#include "Entity/PlayerInventory.h"
#include "Entity/Projectile.h"
#include "Entity/ProjectileFactory.h"
#include "Game/DamageEmitter.h"
#include "Game/Effect/EffectManager.h"
#include "Game/LevelUpManager.h"
#include "Game/Room.h"
#include "MockSystem.h"
#include <cmath>
#include <gtest/gtest.h>

namespace SimpleGame {

// ============================================================
// Test 1: Infinite Pierce (pierce = -1) does not expire on first hit
// ============================================================
TEST(WeaponMechanicsTest, InfinitePierceProjectileDoesNotExpireOnFirstHit)
{
    // Setup monster template
    MonsterInfo mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 100;
    mTmpl.speed = 0.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 0;
    mTmpl.attackCooldown = 1.0f;
    mTmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterInfo(mTmpl);

    // Setup framework and room
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        100,
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    room->StartGame();

    // Create player
    auto player = std::make_shared<Player>(100, 100ULL);
    player->Initialize(100, 100ULL, 100, 5.0f);
    player->SetPos(0.0f, 0.0f);
    player->SetReady(true);
    room->Enter(player);

    // Create two monsters in a line
    auto m1 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 2.0f, 0.0f);
    auto m2 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 4.0f, 0.0f);
    room->GetObjectManager().AddObject(m1);
    room->GetObjectManager().AddObject(m2);

    int32_t m1InitialHp = m1->GetHp();
    int32_t m2InitialHp = m2->GetHp();

    // Create projectile with infinite pierce (-1)
    auto proj = ProjectileFactory::Instance().CreateProjectile(
        room->GetObjectManager(), 100, 999, 1, 1.0f, 0.0f, 20.0f, 0.0f, 50, 5.0f
    );
    proj->SetDamage(50);
    proj->SetPierce(-1);
    proj->SetRadius(0.3f);
    room->GetObjectManager().AddObject(proj);

    // Update to trigger collisions
    for (int i = 0; i < 10; ++i)
        room->Update(0.02f);

    // Verify both monsters were hit and projectile is NOT expired
    EXPECT_LT(m1->GetHp(), m1InitialHp); // Monster 1 took damage
    EXPECT_LT(m2->GetHp(), m2InitialHp); // Monster 2 took damage
    EXPECT_FALSE(proj->IsExpired());     // Infinite pierce projectile should NOT be expired
    EXPECT_FALSE(proj->IsHit());         // OnHit should not consume infinite pierce
}

// ============================================================
// Test 2: speed_mult scales Linear projectile velocity
// ============================================================
TEST(WeaponMechanicsTest, SpeedMultScalesProjectileVelocity)
{
    // Clear weapons for test isolation
    DataManager::Instance().ClearWeaponsForTest();

    // Create synthetic skill with default speed
    SkillInfo skillTmpl;
    skillTmpl.id = 200;
    skillTmpl.name = "speed_test_skill";
    skillTmpl.damage = 10;
    skillTmpl.tickInterval = 1.0f;
    skillTmpl.hitRadius = 2.0f;
    skillTmpl.emitterType = "Linear";
    skillTmpl.typeId = 1;
    DataManager::Instance().AddSkillInfo(skillTmpl);

    // Create synthetic weapon with speed_mult = 2.0
    WeaponInfo weaponTmpl;
    weaponTmpl.id = 200;
    weaponTmpl.name = "Speed Test Weapon";
    weaponTmpl.description = "Weapon with speed multiplier";
    weaponTmpl.maxLevel = 1;

    WeaponLevelInfo level1;
    level1.level = 1;
    level1.skillId = 200;
    level1.damageMult = 1.0f;
    level1.speedMult = 2.0f; // Double speed
    level1.cooldownMult = 1.0f;
    weaponTmpl.levels.push_back(level1);
    DataManager::Instance().AddWeaponInfoForTest(weaponTmpl);

    // Setup room
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        101,
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    room->StartGame();

    // Create player and add weapon with speed multiplier
    auto player = std::make_shared<Player>(101, 101ULL);
    player->Initialize(101, 101ULL, 100, 5.0f);
    player->SetPos(0.0f, 0.0f);
    player->SetReady(true);
    room->Enter(player);

    player->GetInventory().AddOrUpgradeWeapon(200);
    player->RefreshInventoryEffects(room.get());

    // Let emitter fire once
    room->Update(1.1f);

    // Find created projectiles
    auto allObjects = room->GetObjectManager().GetAllObjects();
    bool foundProjectile = false;
    for (const auto &obj : allObjects)
    {
        if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            auto proj = std::dynamic_pointer_cast<Projectile>(obj);
            if (proj)
            {
                foundProjectile = true;
                // Base Linear projectile speed is 15.0f (see DamageEmitter.cpp:270)
                // With speed_mult = 2.0, velocity should be 30.0f
                float speed = std::sqrt(proj->GetVX() * proj->GetVX() + proj->GetVY() * proj->GetVY());
                EXPECT_FLOAT_EQ(speed, 30.0f);
                break;
            }
        }
    }
    EXPECT_TRUE(foundProjectile);
}

// ============================================================
// Test 3: max_targets override caps hits for AoE emitter
// ============================================================
TEST(WeaponMechanicsTest, MaxTargetsOverrideCapsAoEHits)
{
    DataManager::Instance().ClearWeaponsForTest();

    // Create synthetic skill for AoE emitter (pulse damage)
    SkillInfo skillTmpl;
    skillTmpl.id = 201;
    skillTmpl.name = "aoe_test_skill";
    skillTmpl.damage = 10;
    skillTmpl.tickInterval = 0.5f;
    skillTmpl.hitRadius = 5.0f;
    skillTmpl.emitterType = "AoE";
    skillTmpl.maxTargetsPerTick = 99; // High base cap
    DataManager::Instance().AddSkillInfo(skillTmpl);

    // Create synthetic weapon with max_targets = 1 override
    WeaponInfo weaponTmpl;
    weaponTmpl.id = 201;
    weaponTmpl.name = "Single Target AoE";
    weaponTmpl.maxLevel = 1;

    WeaponLevelInfo level1;
    level1.level = 1;
    level1.skillId = 201;
    level1.damageMult = 1.0f;
    level1.maxTargets = 1; // Override to 1 target only
    weaponTmpl.levels.push_back(level1);
    DataManager::Instance().AddWeaponInfoForTest(weaponTmpl);

    // Setup monster template
    MonsterInfo mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 100;
    mTmpl.speed = 0.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 0;
    mTmpl.attackCooldown = 1.0f;
    mTmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterInfo(mTmpl);

    // Setup room
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        102,
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    room->StartGame();

    // Create player and add weapon
    auto player = std::make_shared<Player>(102, 102ULL);
    player->Initialize(102, 102ULL, 100, 5.0f);
    player->SetPos(0.0f, 0.0f);
    player->SetReady(true);
    room->Enter(player);

    player->GetInventory().AddOrUpgradeWeapon(201);
    player->RefreshInventoryEffects(room.get());

    // Create 3 monsters within AoE range (close to player)
    auto m1 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 1.0f, 0.0f);
    auto m2 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 1.5f, 0.0f);
    auto m3 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 2.0f, 0.0f);
    room->GetObjectManager().AddObject(m1);
    room->GetObjectManager().AddObject(m2);
    room->GetObjectManager().AddObject(m3);

    int32_t m1InitialHp = m1->GetHp();
    int32_t m2InitialHp = m2->GetHp();
    int32_t m3InitialHp = m3->GetHp();

    // Update to trigger AoE attack
    room->Update(0.6f);

    // Count how many monsters took damage
    int hitCount = 0;
    if (m1->GetHp() < m1InitialHp)
        hitCount++;
    if (m2->GetHp() < m2InitialHp)
        hitCount++;
    if (m3->GetHp() < m3InitialHp)
        hitCount++;

    // Should only hit 1 target despite having 3 in range
    EXPECT_EQ(hitCount, 1);
}

// ============================================================
// Test 4: Poison DoT effect applies damage over time
// ============================================================
TEST(WeaponMechanicsTest, PoisonDoTAppliesDamageOverTime)
{
    // Setup monster template
    MonsterInfo mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 100;
    mTmpl.speed = 0.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 0;
    mTmpl.attackCooldown = 1.0f;
    mTmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterInfo(mTmpl);

    // Setup room
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        103,
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    room->StartGame();

    // Create player
    auto player = std::make_shared<Player>(103, 103ULL);
    player->Initialize(103, 103ULL, 100, 5.0f);
    player->SetPos(0.0f, 0.0f);
    player->SetReady(true);
    room->Enter(player);

    // Create monster
    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 2.0f, 0.0f);
    room->GetObjectManager().AddObject(monster);

    int32_t initialHp = monster->GetHp();

    // Manually apply poison effect (5 damage per tick)
    Effect::StatusEffect poison;
    poison.type = Effect::Type::POISON;
    poison.sourceId = 103;
    poison.endTime = room->GetTotalRunTime() + 3.0f;
    poison.tickInterval = 0.5f;
    poison.lastTickTime = room->GetTotalRunTime();
    poison.value = 5.0f;
    room->GetEffectManager().ApplyEffect(monster->GetId(), poison);

    // Trigger DoT ticks with multiple updates (damage will occur on each tick)
    for (int i = 0; i < 5; ++i)
    {
        room->Update(0.5f);
    }

    // Poison DoT should have applied damage (exact amount depends on tick timing)
    EXPECT_LT(monster->GetHp(), initialHp);
}

// ============================================================
// Test 5: Player base crit modifies projectile hit calculation
// ============================================================
TEST(WeaponMechanicsTest, PlayerBaseCritModifiesProjectileDamage)
{
    DataManager::Instance().ClearWeaponsForTest();

    // Create synthetic skill
    SkillInfo skillTmpl;
    skillTmpl.id = 203;
    skillTmpl.name = "crit_test_skill";
    skillTmpl.damage = 50;
    skillTmpl.tickInterval = 0.5f;
    skillTmpl.hitRadius = 2.0f;
    skillTmpl.emitterType = "Linear";
    DataManager::Instance().AddSkillInfo(skillTmpl);

    // Setup monster template
    MonsterInfo mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 500;
    mTmpl.speed = 0.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 0;
    mTmpl.attackCooldown = 1.0f;
    mTmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterInfo(mTmpl);

    // Setup room
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        104,
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    room->StartGame();

    // Create player
    auto player = std::make_shared<Player>(104, 104ULL);
    player->Initialize(104, 104ULL, 100, 5.0f);
    player->SetPos(0.0f, 0.0f);
    player->SetReady(true);
    room->Enter(player);

    // Create monster
    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 2.0f, 0.0f);
    room->GetObjectManager().AddObject(monster);

    int32_t initialHp = monster->GetHp();

    // Create projectile
    auto proj = ProjectileFactory::Instance().CreateProjectile(
        room->GetObjectManager(), 104, 203, 1, 1.0f, 0.0f, 20.0f, 0.0f, 50, 2.0f
    );
    proj->SetDamage(50);
    proj->SetPierce(1);
    proj->SetRadius(0.3f);
    room->GetObjectManager().AddObject(proj);

    // Trigger hit
    for (int i = 0; i < 10; ++i)
        room->Update(0.02f);

    // Player has base crit, so damage may vary
    // Just verify damage was applied (not necessarily crit)
    int32_t actualDamage = initialHp - monster->GetHp();
    EXPECT_GT(actualDamage, 0); // Monster should have taken damage
}

// ============================================================
// Test 6: Sparse/out-of-order weapon levels do not crash emitter creation
// ============================================================
TEST(WeaponMechanicsTest, SparseWeaponLevelsDoNotCrashEmitterCreation)
{
    DataManager::Instance().ClearWeaponsForTest();

    // Create synthetic skill
    SkillInfo skillTmpl;
    skillTmpl.id = 204;
    skillTmpl.name = "sparse_level_skill";
    skillTmpl.damage = 10;
    skillTmpl.tickInterval = 0.5f;
    skillTmpl.hitRadius = 2.0f;
    skillTmpl.emitterType = "Linear";
    DataManager::Instance().AddSkillInfo(skillTmpl);

    // Create synthetic weapon with sparse, out-of-order levels
    WeaponInfo weaponTmpl;
    weaponTmpl.id = 204;
    weaponTmpl.name = "Sparse Level Weapon";
    weaponTmpl.maxLevel = 8;

    // Add levels in random order: 8, 1, 3, 5 (missing 2, 4, 6, 7)
    WeaponLevelInfo level1;
    level1.level = 1;
    level1.skillId = 204;
    level1.damageMult = 1.0f;
    weaponTmpl.levels.push_back(level1);

    WeaponLevelInfo level8;
    level8.level = 8;
    level8.skillId = 204;
    level8.damageMult = 2.0f; // Higher damage at level 8
    level8.speedMult = 3.0f;
    weaponTmpl.levels.push_back(level8);

    WeaponLevelInfo level3;
    level3.level = 3;
    level3.skillId = 204;
    level3.damageMult = 1.5f;
    weaponTmpl.levels.push_back(level3);

    WeaponLevelInfo level5;
    level5.level = 5;
    level5.skillId = 204;
    level5.damageMult = 1.8f;
    weaponTmpl.levels.push_back(level5);

    DataManager::Instance().AddWeaponInfoForTest(weaponTmpl);

    // Setup room
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        105,
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    room->StartGame();

    // Create player
    auto player = std::make_shared<Player>(105, 105ULL);
    player->Initialize(105, 105ULL, 100, 5.0f);
    player->SetPos(0.0f, 0.0f);
    player->SetReady(true);
    room->Enter(player);

    for (int i = 0; i < 8; ++i)
    {
        player->GetInventory().AddOrUpgradeWeapon(204);
    }

    // This should NOT crash - it should look up level 8 by value
    player->RefreshInventoryEffects(room.get());

    // Verify emitter was created
    EXPECT_EQ(player->GetEmitterCount(), 1);

    // Let emitter fire and verify speed_mult from level 8 is applied
    room->Update(0.6f);

    // Find created projectiles and verify speed multiplier
    auto allObjects = room->GetObjectManager().GetAllObjects();
    bool foundProjectile = false;
    for (const auto &obj : allObjects)
    {
        if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            auto proj = std::dynamic_pointer_cast<Projectile>(obj);
            if (proj)
            {
                foundProjectile = true;
                // Base speed 15.0f * level 8 speed_mult 3.0f = 45.0f
                float speed = std::sqrt(proj->GetVX() * proj->GetVX() + proj->GetVY() * proj->GetVY());
                EXPECT_FLOAT_EQ(speed, 45.0f);
                break;
            }
        }
    }
    EXPECT_TRUE(foundProjectile);
}

// ============================================================
// Test 7: Level-up integration - emitter refresh → combat tick
// ============================================================
TEST(WeaponMechanicsTest, LevelUpIntegrationTest)
{
    DataManager::Instance().ClearWeaponsForTest();

    // Create synthetic skill for Linear emitter
    SkillInfo skillTmpl;
    skillTmpl.id = 300;
    skillTmpl.name = "integration_test_skill";
    skillTmpl.damage = 20;
    skillTmpl.tickInterval = 0.5f;
    skillTmpl.hitRadius = 2.0f;
    skillTmpl.emitterType = "Linear";
    skillTmpl.typeId = 1;
    DataManager::Instance().AddSkillInfo(skillTmpl);

    // Create synthetic weapon (maxLevel 1 to ensure only one upgrade option)
    WeaponInfo weaponTmpl;
    weaponTmpl.id = 300;
    weaponTmpl.name = "Integration Test Weapon";
    weaponTmpl.description = "Weapon for level-up integration test";
    weaponTmpl.maxLevel = 1;
    weaponTmpl.weight = 100; // High weight to ensure deterministic selection

    WeaponLevelInfo level1;
    level1.level = 1;
    level1.skillId = 300;
    level1.damageMult = 1.0f;
    level1.speedMult = 1.0f;
    level1.cooldownMult = 1.0f;
    weaponTmpl.levels.push_back(level1);
    DataManager::Instance().AddWeaponInfoForTest(weaponTmpl);

    // Setup monster template
    MonsterInfo mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 100;
    mTmpl.speed = 0.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 0;
    mTmpl.attackCooldown = 1.0f;
    mTmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterInfo(mTmpl);

    // Setup room
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        106,
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    room->StartGame();

    // Create player and enter room
    auto player = std::make_shared<Player>(106, 106ULL);
    player->Initialize(106, 106ULL, 100, 5.0f);
    player->SetPos(0.0f, 0.0f);
    player->SetReady(true);
    room->Enter(player);

    // Verify no emitters initially
    EXPECT_EQ(player->GetEmitterCount(), 0);

    // Generate level-up options (should create pool with 1 option for weapon 300)
    LevelUpManager levelUpMgr;
    auto options = levelUpMgr.GenerateOptions(player.get());
    ASSERT_FALSE(options.empty()) << "Should have generated at least one level-up option";

    // Set pending options (required for ApplySelection)
    player->SetPendingLevelUpOptions(options);

    // Apply the first option (weapon 300)
    // This should:
    // 1. Add weapon to inventory
    // 2. Call RefreshInventoryEffects to create emitter
    // 3. Clear pending options
    levelUpMgr.ApplySelection(player.get(), 0, room.get());

    // Verify emitter was created
    EXPECT_EQ(player->GetEmitterCount(), 1) << "Emitter should be created after applying weapon upgrade";

    // Create monster within range
    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 3.0f, 0.0f);
    room->GetObjectManager().AddObject(monster);

    // Tick room to trigger combat
    // The Linear emitter should fire projectiles
    room->Update(0.6f);

    // Count projectiles created
    int projectileCount = 0;
    auto allObjects = room->GetObjectManager().GetAllObjects();
    for (const auto &obj : allObjects)
    {
        if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            projectileCount++;
        }
    }

    // Verify at least one projectile was created
    // Note: Linear emitters fire in random directions, so monster may not be hit
    EXPECT_GT(projectileCount, 0) << "Should have created at least one projectile after level-up";
}

} // namespace SimpleGame
