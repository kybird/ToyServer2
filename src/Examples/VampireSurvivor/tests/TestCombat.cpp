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

TEST(CombatTest, ProjectileHitsMonster)
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

    auto room = std::make_shared<Room>(1, nullptr, nullptr, nullptr, nullptr);

    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 2.0f, 0.0f);
    room->GetObjectManager().AddObject(monster);
    room->_grid.Add(monster);
    int32_t initialHp = monster->GetHp();

    auto proj = ProjectileFactory::Instance().CreateProjectile(
        room->GetObjectManager(), 999, 1, 1, 0.0f, 0.0f, 20.0f, 0.0f, 50, 2.0f
    );
    proj->SetDamage(50);
    room->GetObjectManager().AddObject(proj);
    room->_grid.Add(proj);

    room->Update(0.1f);

    EXPECT_EQ(monster->GetHp(), initialHp - 50);
    EXPECT_EQ(room->GetObjectManager().GetObject(proj->GetId()), nullptr);
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

    auto room = std::make_shared<Room>(2, nullptr, nullptr, nullptr, nullptr);

    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 0.0f, 0.0f);
    monster->SetHp(10);
    room->GetObjectManager().AddObject(monster);
    room->_grid.Add(monster);

    auto proj = ProjectileFactory::Instance().CreateProjectile(
        room->GetObjectManager(), 999, 1, 1, 0.1f, 0.1f, 0.0f, 0.0f, 20, 2.0f
    );
    proj->SetDamage(20);
    room->GetObjectManager().AddObject(proj);
    room->_grid.Add(proj);

    room->Update(0.05f);

    EXPECT_EQ(room->GetObjectManager().GetObject(monster->GetId()), nullptr);
}

TEST(CombatTest, MonsterContactsPlayer)
{
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

    auto room = std::make_shared<Room>(3, nullptr, nullptr, nullptr, nullptr);

    auto player = PlayerFactory::Instance().CreatePlayer(100, 0ULL);
    player->SetPos(0.0f, 0.0f);
    room->_players[100] = player;
    room->GetObjectManager().AddObject(player);
    room->_grid.Add(player);

    auto monster = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 0.5f, 0.0f);
    room->GetObjectManager().AddObject(monster);
    room->_grid.Add(monster);

    EXPECT_EQ(player->GetHp(), 100);

    room->Update(0.1f);

    EXPECT_EQ(player->GetHp(), 80);
    EXPECT_FALSE(monster->CanAttack(room->GetTotalRunTime()));
}

TEST(CombatTest, OverkillDoesNotResultInNegativeHp)
{
    auto room = std::make_shared<Room>(4, nullptr, nullptr, nullptr, nullptr);

    auto player = std::make_shared<Player>(100, 0ULL);
    player->Initialize(100, 0ULL, 100, 5.0f);
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

    auto room = std::make_shared<Room>(5, nullptr, nullptr, nullptr, nullptr);

    auto player = std::make_shared<Player>(100, 0ULL);
    player->Initialize(100, 0ULL, 100, 5.0f);
    player->ApplyInput(1, 1, 0);
    player->SetVelocity(0, 0);
    player->SetReady(true);
    room->Enter(player);

    player->AddEmitter(std::make_shared<DamageEmitter>(1, player));

    auto m1 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 1.0f, 0.0f);
    room->GetObjectManager().AddObject(m1);
    room->_grid.Add(m1);

    auto m2 = MonsterFactory::Instance().CreateMonster(room->GetObjectManager(), 1, 1.5f, 0.0f);
    room->GetObjectManager().AddObject(m2);
    room->_grid.Add(m2);

    room->Update(0.6f);

    EXPECT_EQ(m1->GetHp(), 90);
    EXPECT_EQ(m2->GetHp(), 100);
}

TEST(CombatTest, LinearEmitterRespectsLifetime)
{
    SkillTemplate sTmpl;
    sTmpl.id = 2;
    sTmpl.name = "timed_linear";
    sTmpl.damage = 10;
    sTmpl.tickInterval = 1.0f;
    sTmpl.hitRadius = 2.0f;
    sTmpl.lifeTime = 0.5f;
    sTmpl.emitterType = "Linear";
    sTmpl.maxTargetsPerTick = 1;
    sTmpl.targetRule = "Nearest";
    DataManager::Instance().AddSkillTemplate(sTmpl);

    auto room = std::make_shared<Room>(6, nullptr, nullptr, nullptr, nullptr);
    auto player = std::make_shared<Player>(100, 0ULL);
    player->Initialize(100, 0ULL, 100, 5.0f);
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
    SkillTemplate sTmpl;
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
    DataManager::Instance().AddSkillTemplate(sTmpl);

    auto room = std::make_shared<Room>(10, nullptr, nullptr, nullptr, nullptr);

    auto player = std::make_shared<Player>(100, 0ULL);
    player->Initialize(100, 0ULL, 100, 5.0f);
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
