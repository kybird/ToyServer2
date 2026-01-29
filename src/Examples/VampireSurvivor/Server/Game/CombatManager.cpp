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
        else if (obj->GetType() == Protocol::ObjectType::MONSTER)
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

            // Use radius for collision query
            auto nearby = room->_grid.QueryRange(proj->GetX(), proj->GetY(), proj->GetRadius() + 0.5f);
            for (auto &target : nearby)
            {
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

                    if (distSq <= sumRad * sumRad)
                    {
                        monster->TakeDamage(proj->GetDamage(), room);

                        bool consumed = proj->OnHit();

                        damageEffect.add_target_ids(monster->GetId());
                        damageEffect.add_damage_values(proj->GetDamage());

                        if (monster->IsDead())
                        {
                            // 몬스터 사망 시 경험치 젬 스폰 (즉시 가산 대신)
                            auto gem = std::make_shared<ExpGem>(room->GetObjectManager().GenerateId(), 10);
                            gem->Initialize(gem->GetId(), monster->GetX(), monster->GetY(), 10);

                            room->GetObjectManager().AddObject(gem);
                            room->_grid.Add(gem);

                            // 브로드캐스트를 위해 리스트에 수집 (여기는 즉시 보내도 되지만 구조상 SpawnBroadcast 활용)
                            std::vector<std::shared_ptr<GameObject>> spawns;
                            spawns.push_back(gem);
                            room->BroadcastSpawn(spawns);

                            // LOG_INFO("Monster {} died. Spawned ExpGem {}", monster->GetId(), gem->GetId());
                        }

                        if (consumed)
                        {
                            break; // Projectile consumed, stop checking targets
                        }
                        // Else: Pierce! Continue to next target
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
            // Collision Check (Attack Reach)
            // 시각적으로 접촉하지 않아도 공격 가능 (MONSTER_ATTACK_REACH)
            float sumRad = monster->GetRadius() + player->GetRadius() + GameConfig::MONSTER_ATTACK_REACH;

            if (distSq <= sumRad * sumRad)
            {
                if (monster->CanAttack(room->_totalRunTime))
                {
                    // Player::TakeDamage checks invincibility internally
                    player->TakeDamage(monster->GetContactDamage(), room);
                    monster->ResetAttackCooldown(room->_totalRunTime);

                    // HP Change Packet - TakeDamage 내부에서 HP가 변했을 때만 보내지는 게 맞지만,
                    // 현재 구조상 여기서 보내므로 무적인 경우 HP 변화가 없어도 패킷이 갈 수 있음.
                    // 최적화를 위해 Player HP 변화 체크가 이상적이나,
                    // 로직 단순화를 위해 무적이어도 0데미지 패킷(HP유지) 가는 것은 허용 (동기화)
                    // -> 개선: Player::TakeDamage가 bool을 리턴하거나, HP 변화를 감지해야 함.
                    // 하지만 현재는 그냥 보냄 (큰 문제 없음)
                    Protocol::S_HpChange hpMsg;
                    hpMsg.set_object_id(player->GetId());
                    hpMsg.set_current_hp(player->GetHp());
                    hpMsg.set_max_hp(player->GetMaxHp());
                    S_HpChangePacket hpPkt(std::move(hpMsg));
                    room->BroadcastPacket(hpPkt);

                    // Apply Knockback - REMOVED per user request
                    // ApplyKnockback(monster, player, room);

                    if (player->IsDead())
                    {
                        LOG_INFO("Player {} has died in Room {}", player->GetId(), room->GetId());
                        Protocol::S_PlayerDead deadMsg;
                        deadMsg.set_player_id(player->GetId());
                        S_PlayerDeadPacket deadPkt(deadMsg);
                        room->BroadcastPacket(deadPkt);

                        // Check Game Over (Loss Condition)
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
