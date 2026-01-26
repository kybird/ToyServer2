#include "Game/DamageEmitter.h"
#include "Common/GamePackets.h"
#include "Core/DataManager.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Entity/ProjectileFactory.h"
#include "Game/Room.h"
#include "Math/Vector2.h"
#include "System/ILog.h"
#include <algorithm>
#include <limits>

namespace SimpleGame {

DamageEmitter::DamageEmitter(int32_t skillId, std::shared_ptr<Player> owner, int32_t weaponId, int32_t level)
    : _skillId(skillId), _owner(owner), _weaponId(weaponId), _level(level)
{
    const auto *tmpl = DataManager::Instance().GetSkillTemplate(skillId);
    if (tmpl)
    {
        _damage = tmpl->damage;
        _typeId = tmpl->typeId;
        _tickInterval = tmpl->tickInterval;
        _hitRadius = tmpl->hitRadius;
        _emitterType = tmpl->emitterType;
        _pierce = tmpl->pierce;
        _maxTargetsPerTick = tmpl->maxTargetsPerTick;
        _targetRule = tmpl->targetRule;
        _lifeTime = tmpl->lifeTime;

        LOG_INFO(
            "[DamageEmitter] Created: Skill={} Type={} typeId={} Interval={:.2f}s Owner={}",
            _skillId,
            _emitterType,
            _typeId,
            _tickInterval,
            owner ? owner->GetId() : -1
        );
    }
    else
    {
        LOG_ERROR("[DamageEmitter] Failed to find SkillTemplate for ID {}", skillId);
    }
    _timer = _tickInterval; // Ready to fire
}

void DamageEmitter::Update(float dt, Room *room)
{
    auto owner = _owner.lock();
    if (!owner || owner->IsDead() || !room || !_active)
        return;

    _elapsedTime += dt;
    if (_lifeTime > 0.0f && _elapsedTime >= _lifeTime)
    {
        LOG_INFO("[DamageEmitter] Expired: Skill={} Owner={}", _skillId, owner->GetId());
        _active = false;
        return;
    }

    // --- Apply Multipliers ---
    float effectiveDamageMult = owner->GetDamageMultiplier();
    float effectiveCooldownMult = owner->GetCooldownMultiplier();
    float effectiveAreaMult = owner->GetAreaMultiplier();
    int32_t additionalProjectiles = owner->GetAdditionalProjectileCount(_weaponId);
    int32_t additionalPierce = owner->GetAdditionalPierceCount(_weaponId); // [Modified] Pass WeaponID

    // Apply Base Pierce + Bonus
    // Note: _pierce is the emitter's instance variable initialized from template.
    // If we want to dynamically update it, we should do it here or in constructor?
    // Constructor initialized it. Here we are in Update.
    // If we modify _pierce member here, it persists? No, _pierce is member.
    // But Update runs every frame. If we do _pierce += bonus, it will explode.
    // We should compute 'effectivePierce'.
    // BUT DamageEmitter passes _pierce to ProjectileFactory.
    // We need to calculate final pierce here and pass it.

    // HOWEVER, ProjectileFactory uses _pierce member of DamageEmitter?
    // Let's check where Projectile is created.
    // Line 156: ProjectileFactory::Instance().CreateProjectile(..., _skillId, ...)
    // Wait, CreateProjectile signature does NOT take 'pierce'.
    // It takes SkillId. The Projectile usually reads stats from SkillTemplate OR we override it.

    // Let's check ProjectileFactory signature in Step 689 Code.
    /*
    auto proj = ProjectileFactory::Instance().CreateProjectile(
                    room->_objMgr,
                    owner->GetId(),
                    _skillId,
                    _typeId,
                    spawnX, spawnY, vx, vy,
                    finalDamage,
                    life
                );
    */
    // It does not take 'pierce'.
    // So the Projectile probably initializes itself from SkillTemplate inside CreateProjectile.
    // OR we need to SET PIERCE on the created projectile.

    // Let's scroll down to Line 169 in DamageEmitter.cpp
    /*
    if (proj) {
       proj->SetRadius(0.2f);
       // We should set Pierce here!
    }
    */

    // So I need to add 'proj->SetPierce(_pierce + additionalPierce)' access.

    // Weapon Level Multipliers
    if (_weaponId > 0)
    {
        const auto *weaponTmpl = DataManager::Instance().GetWeaponTemplate(_weaponId);
        if (weaponTmpl && _level > 0 && _level <= static_cast<int>(weaponTmpl->levels.size()))
        {
            const auto &levelData = weaponTmpl->levels[_level - 1];
            effectiveDamageMult *= levelData.damageMult;
            effectiveCooldownMult *= levelData.cooldownMult;
        }
    }

    float currentTickInterval = _tickInterval * effectiveCooldownMult;
    // Clamp min interval
    currentTickInterval = std::max(0.05f, currentTickInterval);

    _timer += dt;
    if (_timer >= currentTickInterval)
    {
        _timer -= currentTickInterval;

        float px = owner->GetX();
        float py = owner->GetY();
        int32_t finalDamage = static_cast<int32_t>(_damage * effectiveDamageMult);
        float finalRadius = _hitRadius * effectiveAreaMult;

        if (_emitterType == "Linear")
        {
            Vector2 direction = owner->GetFacingDirection();

            // Auto-Targeting
            if (_targetRule == "Nearest")
            {
                auto monsters = room->GetMonstersInRange(px, py, 30.0f); // Search 30m
                if (!monsters.empty())
                {
                    std::shared_ptr<Monster> nearest = nullptr;
                    float minDistSq = std::numeric_limits<float>::max();

                    for (auto &monster : monsters)
                    {
                        if (monster->IsDead())
                            continue;
                        float dSq = Vector2::DistanceSq(Vector2(px, py), Vector2(monster->GetX(), monster->GetY()));
                        if (dSq < minDistSq)
                        {
                            minDistSq = dSq;
                            nearest = monster;
                        }
                    }

                    if (nearest)
                    {
                        direction = Vector2(nearest->GetX() - px, nearest->GetY() - py);
                        if (!direction.IsZero())
                        {
                            direction.Normalize();
                        }
                        else
                        {
                            direction = owner->GetFacingDirection();
                        }
                    }
                }
            }

            float speed = 15.0f;
            float life = 3.0f;
            int32_t projectileCount = 1 + additionalProjectiles;

            for (int i = 0; i < projectileCount; ++i)
            {
                // Spawn slightly ahead of player (add slight delay or offset if firing many?)
                // For now, same spot/direction or slightly spread?
                // Let's just fire them all or with slight angle offset
                Vector2 fireDir = direction;
                if (projectileCount > 1)
                {
                    // Spread: -15 to +15 degrees
                    float angleOffset = (static_cast<float>(i) / (projectileCount - 1) - 0.5f) * 0.5f;
                    float s = sin(angleOffset);
                    float c = cos(angleOffset);
                    fireDir = Vector2(direction.x * c - direction.y * s, direction.x * s + direction.y * c);
                }

                float spawnOffset = owner->GetRadius() + 0.3f;
                float spawnX = px + fireDir.x * spawnOffset;
                float spawnY = py + fireDir.y * spawnOffset;

                auto proj = ProjectileFactory::Instance().CreateProjectile(
                    room->_objMgr,
                    owner->GetId(),
                    _skillId,
                    _typeId,
                    spawnX,
                    spawnY,
                    fireDir.x * speed,
                    fireDir.y * speed,
                    finalDamage,
                    life
                );

                if (proj)
                {
                    proj->SetRadius(0.2f);
                    // [Pierce] Set total pierce count (Base + Bonus)
                    // _pierce is base from SkillTemplate
                    proj->SetPierce(_pierce + additionalPierce);

                    room->_objMgr.AddObject(proj);
                    room->_grid.Add(proj);
                    room->BroadcastSpawn({proj});
                }
            }
        }
        else
        {
            // AoE Damage logic
            auto victims = room->GetMonstersInRange(px, py, finalRadius);
            if (_targetRule == "Nearest")
            {
                std::sort(
                    victims.begin(),
                    victims.end(),
                    [&](const auto &a, const auto &b)
                    {
                        return Vector2::DistanceSq(Vector2(px, py), Vector2(a->GetX(), a->GetY())) <
                               Vector2::DistanceSq(Vector2(px, py), Vector2(b->GetX(), b->GetY()));
                    }
                );
            }

            int count = 0;
            int32_t finalMaxTargets = _maxTargetsPerTick + additionalProjectiles;
            std::vector<int32_t> hitTargetIds;
            std::vector<int32_t> hitDamageValues;

            for (auto &monster : victims)
            {
                if (count >= finalMaxTargets)
                    break;

                monster->TakeDamage(finalDamage, room);
                hitTargetIds.push_back(monster->GetId());
                hitDamageValues.push_back(finalDamage);
                count++;
            }

            if (!hitTargetIds.empty())
            {
                // 1. Broadcast Visual Effect
                Protocol::S_SkillEffect skillMsg;
                skillMsg.set_caster_id(owner->GetId());
                skillMsg.set_skill_id(_skillId);
                skillMsg.set_x(px);
                skillMsg.set_y(py);
                for (int32_t tid : hitTargetIds)
                    skillMsg.add_target_ids(tid);
                room->BroadcastPacket(S_SkillEffectPacket(std::move(skillMsg)));

                // 2. Broadcast Damage Numbers
                Protocol::S_DamageEffect damageMsg;
                damageMsg.set_skill_id(_skillId);
                for (int32_t tid : hitTargetIds)
                    damageMsg.add_target_ids(tid);
                for (int32_t dmg : hitDamageValues)
                    damageMsg.add_damage_values(dmg);
                room->BroadcastPacket(S_DamageEffectPacket(std::move(damageMsg)));
            }
        }
    }
}

bool DamageEmitter::IsExpired() const
{
    if (!_active)
        return true;
    auto owner = _owner.lock();
    return !owner || owner->IsDead();
}

} // namespace SimpleGame
