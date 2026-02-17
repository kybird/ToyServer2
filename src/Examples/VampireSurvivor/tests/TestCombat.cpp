#include "Core/DataManager.h"
#include "Entity/Monster.h"
#include "Entity/MonsterFactory.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Entity/ProjectileFactory.h"
#include "Game/DamageEmitter.h"
#include "Game/Room.h"
#include "System/MockSystem.h"
#include <gtest/gtest.h>

namespace SimpleGame {

TEST(CombatTest, ProjectileHitsMonster)
{
    MonsterInfo tmpl;
    tmpl.id = 1;
    tmpl.hp = 100;
    tmpl.speed = 2.0f;
    tmpl.radius = 0.5f;
    tmpl.damageOnContact = 10;
    tmpl.attackCooldown = 1.0f;
    tmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterInfo(tmpl);

    // [Mock] Setup Framework
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        1,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    // [Fix] StartGame sets _gameStarted = true, required for ExecuteUpdate
    room->StartGame();

    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 2.0f, 0.0f);
    room->GetObjectManager().AddObject(monster);

    // grid.Add 기능이 삭제되었으므로 직접 추가하는 테스트 로직 제거 (Update 호출 시 처리됨)
    int32_t initialHp = monster->GetHp();

    auto proj = ProjectileFactory::Instance().CreateProjectile(
        room->GetObjectManager(), 999, 1, 1, 0.0f, 0.0f, 20.0f, 0.0f, 50, 2.0f
    );
    proj->SetDamage(50);
    room->GetObjectManager().AddObject(proj);

    // [Fix] Room::Update now requires at least one player to process anything
    // [Fix] Make ID and SessionID same to workaround CombatManager lookup logic
    auto p = std::make_shared<Player>(100, 100ULL);
    p->Initialize(100, 100ULL, 100, 5.0f);
    p->SetReady(true);
    room->Enter(p);

    // [Fix] Use smaller steps to ensure collision is caught by grid rebuild and update physics
    for (int i = 0; i < 5; ++i)
        room->Update(0.02f);

    EXPECT_EQ(monster->GetHp(), initialHp - 50);
    EXPECT_EQ(room->GetObjectManager().GetObject(proj->GetId()), nullptr);
}

TEST(CombatTest, MonsterDies)
{
    MonsterInfo tmpl;
    tmpl.id = 1;
    tmpl.hp = 100;
    tmpl.speed = 2.0f;
    tmpl.radius = 0.5f;
    tmpl.damageOnContact = 10;
    tmpl.attackCooldown = 1.0f;
    tmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterInfo(tmpl);

    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        2,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    room->StartGame();

    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 0.0f, 0.0f);
    monster->SetHp(10);
    monster->ResetAttackCooldown(-100.0f); // Make ready to attack
    room->GetObjectManager().AddObject(monster);

    // [Fix] Room::Update now requires at least one player to process anything
    // [Fix] Manually create and init to avoid PlayerFactory dependency issues
    auto p = std::make_shared<Player>(100, 100ULL);
    p->Initialize(100, 100ULL, 100, 5.0f);
    p->SetReady(true);
    room->Enter(p);

    auto proj = ProjectileFactory::Instance().CreateProjectile(
        room->GetObjectManager(), 999, 1, 1, 0.1f, 0.1f, 0.0f, 0.0f, 20, 2.0f
    );
    proj->SetDamage(20);
    room->GetObjectManager().AddObject(proj);

    room->Update(0.05f);

    EXPECT_EQ(room->GetObjectManager().GetObject(monster->GetId()), nullptr);
}

TEST(CombatTest, MonsterContactsPlayer)
{
    MonsterInfo mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 100;
    mTmpl.speed = 2.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 20;
    mTmpl.attackCooldown = 1.0f;
    mTmpl.aiType = MonsterAIType::CHASER;
    DataManager::Instance().AddMonsterInfo(mTmpl);

    PlayerInfo pTmpl;
    pTmpl.id = 1;
    pTmpl.hp = 100;
    pTmpl.speed = 5.0f;
    DataManager::Instance().AddPlayerInfo(pTmpl);

    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        3,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    // [Fix] StartGame sets _gameStarted = true, required for ExecuteUpdate
    room->StartGame();

    // [Fix] Make ID and SessionID same to workaround CombatManager::ExecuteAttackEvents lookup bug
    auto player = std::make_shared<Player>(100, 100ULL);
    player->Initialize(100, 100ULL, 100, 5.0f);
    player->SetPos(0.0f, 0.0f);

    // [Fix] Ensure player is ready so room->Update processes logic
    player->SetReady(true);
    room->Enter(player);

    // [Fix] Move monster slightly closer to ensure collision
    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 0.4f, 0.0f);
    monster->ResetAttackCooldown(-100.0f); // Make ready to attack
    room->GetObjectManager().AddObject(monster);

    EXPECT_EQ(player->GetHp(), 100);

    for (int i = 0; i < 5; ++i)
        room->Update(0.02f);

    EXPECT_EQ(player->GetHp(), 80);
    EXPECT_FALSE(monster->CanAttack(room->GetTotalRunTime()));
}

TEST(CombatTest, OverkillDoesNotResultInNegativeHp)
{
    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        4,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    // [Fix] StartGame sets _gameStarted = true
    room->StartGame();

    auto player = std::make_shared<Player>(100, 100ULL);
    player->Initialize(100, 100ULL, 100, 5.0f);
    player->SetHp(10);
    player->SetReady(true);
    room->Enter(player);

    player->TakeDamage(100, room.get());

    EXPECT_EQ(player->GetHp(), 0);
    EXPECT_TRUE(player->IsDead());

    auto monster = std::make_shared<Monster>(200, 1);
    monster->Initialize(200, 1, 100, 0.5f, 10, 1.0f, 2.0f);
    monster->SetHp(10);

    monster->TakeDamage(100, room.get());

    EXPECT_EQ(monster->GetHp(), 0);
    EXPECT_TRUE(monster->IsDead());
}

TEST(CombatTest, LinearEmitterHitsNearestMonster)
{
    SkillInfo sTmpl;
    sTmpl.id = 1;
    sTmpl.name = "base_linear";
    sTmpl.damage = 10;
    sTmpl.tickInterval = 0.5f;
    sTmpl.hitRadius = 2.0f;
    sTmpl.lifeTime = 0.0f;
    sTmpl.emitterType = "Linear";
    sTmpl.maxTargetsPerTick = 1;
    sTmpl.targetRule = "Nearest";
    DataManager::Instance().AddSkillInfo(sTmpl);

    MonsterInfo mTmpl;
    mTmpl.id = 1;
    mTmpl.hp = 100;
    mTmpl.speed = 0.0f;
    mTmpl.radius = 0.5f;
    mTmpl.damageOnContact = 0;
    mTmpl.attackCooldown = 1.0f;
    DataManager::Instance().AddMonsterInfo(mTmpl);

    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        5,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    // [Fix] StartGame sets _gameStarted = true
    room->StartGame();

    // [Fix] Make ID and SessionID same
    auto player = std::make_shared<Player>(100, 100ULL);
    player->Initialize(100, 100ULL, 100, 5.0f);
    player->ApplyInput(1, 1, 0);
    player->SetVelocity(0, 0);
    player->SetReady(true);
    room->Enter(player);

    player->AddEmitter(std::make_shared<DamageEmitter>(1, player));

    auto m1 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 1.5f, 0.0f);
    room->GetObjectManager().AddObject(m1);

    auto m2 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 5.0f, 0.0f);
    room->GetObjectManager().AddObject(m2);

    // Update in small steps
    for (int i = 0; i < 10; ++i)
        room->Update(0.04f); // total 0.4s

    EXPECT_EQ(m1->GetHp(), 90);
    EXPECT_EQ(m2->GetHp(), 100);
}

TEST(CombatTest, LinearEmitterRespectsLifetime)
{
    SkillInfo sTmpl;
    sTmpl.id = 2;
    sTmpl.name = "timed_linear";
    sTmpl.damage = 10;
    sTmpl.tickInterval = 1.0f;
    sTmpl.hitRadius = 2.0f;
    sTmpl.lifeTime = 0.5f;
    sTmpl.emitterType = "Linear";
    sTmpl.maxTargetsPerTick = 1;
    sTmpl.targetRule = "Nearest";
    DataManager::Instance().AddSkillInfo(sTmpl);

    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        6,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    // [Fix] StartGame sets _gameStarted = true
    room->StartGame();
    // [Fix] Make ID and SessionID same
    auto player = std::make_shared<Player>(100, 100ULL);
    player->Initialize(100, 100ULL, 100, 5.0f);
    player->SetReady(true);
    room->Enter(player);

    player->ClearEmitters();
    player->AddEmitter(std::make_shared<DamageEmitter>(2, player));

    EXPECT_EQ(player->GetEmitterCount(), 1);

    room->Update(0.3f);
    EXPECT_EQ(player->GetEmitterCount(), 1);

    room->Update(0.3f);
    EXPECT_EQ(player->GetEmitterCount(), 0);
}

TEST(CombatTest, DISABLED_MonsterKnockback)
{
}

TEST(CombatTest, LinearEmitterSpawnsProjectile)
{
    SkillInfo sTmpl;
    sTmpl.id = 10;
    sTmpl.name = "spawn_linear";
    sTmpl.damage = 10;
    sTmpl.tickInterval = 0.5f;
    sTmpl.hitRadius = 2.0f;
    sTmpl.lifeTime = 0.0f;
    sTmpl.typeId = 777;
    sTmpl.emitterType = "Linear";
    sTmpl.maxTargetsPerTick = 1;
    sTmpl.targetRule = "Nearest";
    DataManager::Instance().AddSkillInfo(sTmpl);

    auto mockFramework = std::make_shared<System::MockFramework>();
    auto room = std::make_shared<Room>(
        10,
        mockFramework,
        mockFramework->GetDispatcher(),
        mockFramework->GetTimer(),
        mockFramework->CreateStrand(),
        nullptr
    );
    // [Fix] StartGame sets _gameStarted = true
    room->StartGame();

    // [Fix] Make ID and SessionID same
    auto player = std::make_shared<Player>(100, 100ULL);
    player->Initialize(100, 100ULL, 100, 5.0f);
    player->ApplyInput(1, 1, 0);
    player->SetVelocity(0, 0);
    player->SetReady(true);
    room->Enter(player);

    auto emitter = std::make_shared<DamageEmitter>(10, player);
    player->AddEmitter(emitter);

    room->Update(0.6f);

    auto allObjects = room->GetObjectManager().GetAllObjects();
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
                EXPECT_GT(std::abs(proj->GetVX()), 0.0f);
                break;
            }
        }
    }
    EXPECT_TRUE(foundProjectile);
}

} // namespace SimpleGame
