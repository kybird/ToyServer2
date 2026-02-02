#include "CombatManager.h"
#include "Entity/ExpGem.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Game/ObjectManager.h"
#include "Game/Room.h"
#include "Game/SpatialGrid.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"

namespace SimpleGame {

CombatManager::CombatManager()
{
}
CombatManager::~CombatManager()
{
}

void CombatManager::Update(float dt, Room *room)
{
    ResolveProjectileCollisions(dt, room);
    ResolveBodyCollisions(dt, room);
    ResolveItemCollisions(dt, room);
    ResolveCleanup(room);
}

void CombatManager::ResolveCleanup(Room *room)
{
    // Handle all cleanup (projectiles, monsters, items, etc.)
    auto objects = room->_objMgr.GetAllObjects();
    std::vector<int32_t> despawnIds;
    std::vector<int32_t> pickerIds;

    for (auto &obj : objects)
    {
        bool shouldRemove = false;
        int32_t pickerId = 0;

        if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            auto proj = std::static_pointer_cast<Projectile>(obj);
            if (proj->IsExpired())
                shouldRemove = true;
        }
        if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {

            auto monster = std::static_pointer_cast<Monster>(obj);
            if (monster->IsDead())
                shouldRemove = true;
        }
        else if (obj->GetType() == Protocol::ObjectType::ITEM)
        {
            auto gem = std::static_pointer_cast<ExpGem>(obj);
            if (gem->IsPickedUp())
            {
                shouldRemove = true;
                pickerId = gem->GetPickerId();
            }
        }

        if (shouldRemove)
        {
            despawnIds.push_back(obj->GetId());
            pickerIds.push_back(pickerId);
            room->_grid.Remove(obj);
            room->_objMgr.RemoveObject(obj->GetId());
        }
    }

    if (!despawnIds.empty())
    {
        room->BroadcastDespawn(despawnIds, pickerIds);
    }
}

void CombatManager::ResolveProjectileCollisions(float dt, Room *room)
{
    auto objects = room->_objMgr.GetAllObjects();
    Protocol::S_DamageEffect damageEffect;

    for (auto &obj : objects)
    {
        if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            auto proj = std::static_pointer_cast<Projectile>(obj);
            if (proj->IsExpired())
                continue;

            // [Optimization] Increase query radius for fast-moving projectiles
            room->_queryBuffer.clear();
            room->_grid.QueryRange(
                proj->GetX(), proj->GetY(), proj->GetRadius() + 1.5f, room->_queryBuffer, room->_objMgr
            );

            // Use GameObject (Single Path)
            for (auto &target : room->_queryBuffer)
            {
                if (proj->IsExpired())
                    break;

                if (target->GetId() == proj->GetId())
                    continue;

                // Projectiles never hit any player (self or others)
                if (target->GetType() == Protocol::ObjectType::PLAYER)
                    continue;

                if (target->GetType() == Protocol::ObjectType::MONSTER)
                {
                    auto monster = std::static_pointer_cast<Monster>(target);
                    if (monster->IsDead())
                        continue;

                    float dx = proj->GetX() - monster->GetX();
                    float dy = proj->GetY() - monster->GetY();
                    float distSq = dx * dx + dy * dy;
                    float sumRad = proj->GetRadius() + monster->GetRadius();

                    // [Precision] Using a slight margin for hit detection stability
                    if (distSq <= (sumRad + 0.1f) * (sumRad + 0.1f))
                    {
                        monster->TakeDamage(proj->GetDamage(), room);

                        bool consumed = proj->OnHit();

                        damageEffect.add_target_ids(monster->GetId());
                        damageEffect.add_damage_values(proj->GetDamage());

                        if (monster->IsDead())
                        {
                            // 몬스터 사망 시 경험치 젬 스폰
                            auto gem = std::make_shared<ExpGem>(room->GetObjectManager().GenerateId(), 10);
                            gem->Initialize(gem->GetId(), monster->GetX(), monster->GetY(), 10);

                            room->GetObjectManager().AddObject(gem);
                            room->_grid.Add(gem);

                            std::vector<std::shared_ptr<GameObject>> spawns;
                            spawns.push_back(gem);
                            room->BroadcastSpawn(spawns);
                        }

                        if (consumed)
                        {
                            break; // Projectile consumed, stop checking targets
                        }
                    }
                }
            }
        }
    }

    if (damageEffect.target_ids_size() > 0)
    {
        S_DamageEffectPacket damagePkt(std::move(damageEffect));
        room->BroadcastPacket(damagePkt);
    }
}

void CombatManager::ResolveBodyCollisions(float dt, Room *room)
{
    // Pass 1: 충돌 판정 및 이벤트 수집 (SIMD-Ready)
    attackEventBuffer_.clear();
    CollectAttackEvents(room, attackEventBuffer_);

    // Pass 2: 수집된 이벤트 처리 (Side Effects)
    ExecuteAttackEvents(room, attackEventBuffer_);
}

void CombatManager::CollectAttackEvents(Room *room, std::vector<AttackEvent> &outEvents)
{
    // Single Path (GameObject)
    auto objects = room->_objMgr.GetAllObjects();

    for (auto &obj : objects)
    {
        if (obj->GetType() != Protocol::ObjectType::MONSTER)
            continue;

        auto monster = std::dynamic_pointer_cast<Monster>(obj);
        if (!monster || monster->IsDead())
            continue;

        for (auto &playerPair : room->_players)
        {
            auto player = playerPair.second;
            if (player->IsDead())
                continue;

            float dx = monster->GetX() - player->GetX();
            float dy = monster->GetY() - player->GetY();
            float distSq = dx * dx + dy * dy;
            float sumRad = monster->GetRadius() + player->GetRadius() + GameConfig::MONSTER_ATTACK_REACH;

            if (distSq <= sumRad * sumRad)
            {
                if (monster->CanAttack(room->_totalRunTime))
                {
                    AttackEvent evt;
                    evt.monsterIndex = SIZE_MAX;
                    evt.monsterId = monster->GetId();
                    evt.playerId = player->GetId();
                    evt.damage = monster->GetContactDamage();
                    evt.attackTime = room->_totalRunTime;
                    outEvents.push_back(evt);
                }
            }
        }
    }
}

void CombatManager::ExecuteAttackEvents(Room *room, const std::vector<AttackEvent> &events)
{

    for (const auto &evt : events)
    {
        // 플레이어 찾기
        auto playerIt = room->_players.find(evt.playerId);
        if (playerIt == room->_players.end())
            continue;

        auto player = playerIt->second;
        if (player->IsDead())
            continue;

        // 데미지 적용
        player->TakeDamage(evt.damage, room);

        // 쿨다운 갱신
        auto monsterObj = room->_objMgr.GetObject(evt.monsterId);
        if (monsterObj && monsterObj->GetType() == Protocol::ObjectType::MONSTER)
        {
            auto monster = std::static_pointer_cast<Monster>(monsterObj);
            monster->ResetAttackCooldown(evt.attackTime);
        }

        // HP 변화 패킷 전송
        Protocol::S_HpChange hpMsg;
        hpMsg.set_object_id(player->GetId());
        hpMsg.set_current_hp(player->GetHp());
        hpMsg.set_max_hp(player->GetMaxHp());
        S_HpChangePacket hpPkt(std::move(hpMsg));
        room->BroadcastPacket(hpPkt);

        // 플레이어 사망 처리
        if (player->IsDead())
        {
            LOG_INFO("Player {} has died in Room {}", player->GetId(), room->GetId());
            Protocol::S_PlayerDead deadMsg;
            deadMsg.set_player_id(player->GetId());
            S_PlayerDeadPacket deadPkt(deadMsg);
            room->BroadcastPacket(deadPkt);

            // Game Over 체크
            bool allDead = true;
            for (auto &pPair : room->_players)
            {
                if (!pPair.second->IsDead())
                {
                    allDead = false;
                    break;
                }
            }

            if (allDead)
            {
                room->HandleGameOver(false);
            }
        }
    }
}

void CombatManager::ResolveItemCollisions(float dt, Room *room)
{
    auto objects = room->_objMgr.GetAllObjects();

    for (auto &obj : objects)
    {
        if (obj->GetType() != Protocol::ObjectType::ITEM)
            continue;

        auto gem = std::static_pointer_cast<ExpGem>(obj);
        if (gem->IsPickedUp())
            continue;

        // 가장 가까운 플레이어 찾기
        auto nearestPlayer = room->GetNearestPlayer(gem->GetX(), gem->GetY());
        if (nearestPlayer == nullptr || nearestPlayer->IsDead())
            continue;

        float dx = nearestPlayer->GetX() - gem->GetX();
        float dy = nearestPlayer->GetY() - gem->GetY();
        float distSq = dx * dx + dy * dy;

        // 자석 범위 안이면 즉시 습득 처리 (연출은 클라이언트가 담당)
        if (distSq <= GameConfig::EXP_GEM_MAGNET_RADIUS * GameConfig::EXP_GEM_MAGNET_RADIUS)
        {
            nearestPlayer->AddExp(gem->GetExpAmount(), room);
            gem->SetPickerId(nearestPlayer->GetId());
            gem->MarkAsPickedUp();

            // LOG_DEBUG(
            //     "Player {} magnetized ExpGem {}. EXP +{}", nearestPlayer->GetId(), gem->GetId(), gem->GetExpAmount()
            // );
        }
    }
}
} // namespace SimpleGame
