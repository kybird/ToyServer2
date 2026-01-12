#include "CombatManager.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Game/ObjectManager.h"
#include "Game/Room.h"
#include "Game/SpatialGrid.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include <cmath>

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
    ResolveCleanup(room);
}

void CombatManager::ResolveCleanup(Room *room)
{
    auto objects = room->_objMgr.GetAllObjects();
    std::vector<int32_t> despawnIds;

    for (auto &obj : objects)
    {
        bool shouldRemove = false;
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

        if (shouldRemove)
        {
            despawnIds.push_back(obj->GetId());
            room->_grid.Remove(obj);
            room->_objMgr.RemoveObject(obj->GetId());
        }
    }

    if (!despawnIds.empty())
    {
        room->BroadcastDespawn(despawnIds);
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
                        proj->SetHit();

                        damageEffect.add_target_ids(monster->GetId());
                        damageEffect.add_damage_values(proj->GetDamage());

                        if (monster->IsDead())
                        {
                            auto it = room->_players.find(proj->GetOwnerId());
                            if (it != room->_players.end())
                            {
                                it->second->AddExp(10, room);
                            }
                        }
                        break;
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
            float sumRad = monster->GetRadius() + player->GetRadius();

            if (distSq <= sumRad * sumRad)
            {
                if (monster->CanAttack(room->_totalRunTime))
                {
                    player->TakeDamage(monster->GetContactDamage(), room);
                    monster->ResetAttackCooldown(room->_totalRunTime);

                    // HP Change Packet
                    Protocol::S_HpChange hpMsg;
                    hpMsg.set_object_id(player->GetId());
                    hpMsg.set_current_hp(player->GetHp());
                    hpMsg.set_max_hp(player->GetMaxHp());
                    S_HpChangePacket hpPkt(std::move(hpMsg));
                    room->BroadcastPacket(hpPkt);

                    // Apply Knockback
                    ApplyKnockback(monster, player);

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
                            LOG_INFO("All players died. Game Over in Room {}", room->GetId());
                            Protocol::S_GameOver gameOverMsg;
                            gameOverMsg.set_survived_time_ms(static_cast<int64_t>(room->_totalRunTime * 1000));
                            gameOverMsg.set_is_win(false);
                            S_GameOverPacket gameOverPkt(gameOverMsg);
                            room->BroadcastPacket(gameOverPkt);
                        }
                    }
                }
            }
        }
    }
}

void CombatManager::ApplyKnockback(std::shared_ptr<Monster> monster, std::shared_ptr<Player> player)
{
    float dx = monster->GetX() - player->GetX();
    float dy = monster->GetY() - player->GetY();
    float len = std::sqrt(dx * dx + dy * dy);

    if (len > 0)
    {
        dx /= len;
        dy /= len;
        monster->SetVelocity(dx * KNOCKBACK_FORCE, dy * KNOCKBACK_FORCE);
        LOG_DEBUG(
            "Monster {} knocked back from Player {}. Dir=({:.2f}, {:.2f})", monster->GetId(), player->GetId(), dx, dy
        );
    }
}

} // namespace SimpleGame
