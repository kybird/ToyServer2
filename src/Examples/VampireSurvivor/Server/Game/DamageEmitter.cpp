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
        _activeDuration = tmpl->activeDuration;
        _dotInterval = tmpl->dotInterval;

        LOG_INFO(
            "[DamageEmitter] Created: Skill={} Type={} typeId={} Interval={:.2f}s Duration={:.1f}s Owner={}",
            _skillId,
            _emitterType,
            _typeId,
            _tickInterval,
            _activeDuration,
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
    float effectiveDurationMult = owner->GetDurationMultiplier();
    int32_t additionalProjectiles = owner->GetAdditionalProjectileCount(_weaponId);
    int32_t additionalPierce = owner->GetAdditionalPierceCount(_weaponId);

    // Weapon Level Multipliers
    if (_weaponId > 0)
    {
        const auto *weaponTmpl = DataManager::Instance().GetWeaponTemplate(_weaponId);
        if (weaponTmpl && _level > 0 && _level <= static_cast<int>(weaponTmpl->levels.size()))
        {
            const auto &levelData = weaponTmpl->levels[_level - 1];
            effectiveDamageMult *= levelData.damageMult;
            effectiveCooldownMult *= levelData.cooldownMult;
            effectiveDurationMult *= levelData.durationMult;
        }
    }

    float currentTickInterval = _tickInterval * effectiveCooldownMult;
    currentTickInterval = std::max(0.05f, currentTickInterval);

    float effectiveActiveDuration = _activeDuration * effectiveDurationMult;

    // --- State-Based Field Logic (e.g. Frost Nova) ---
    if (_activeDuration > 0.0f)
    {
        _timer += dt;

        if (_state == EmitterState::COOLING)
        {
            if (_timer >= currentTickInterval)
            {
                _state = EmitterState::ACTIVE;
                _timer = 0.0f;
                _dotTimer = _dotInterval; // Trigger first DoT immediately

                // 1. Broadcast Visual Start
                Protocol::S_SkillEffect skillMsg;
                skillMsg.set_caster_id(owner->GetId());
                skillMsg.set_skill_id(_skillId);
                skillMsg.set_x(owner->GetX());
                skillMsg.set_y(owner->GetY());
                skillMsg.set_radius(_hitRadius * effectiveAreaMult);
                skillMsg.set_duration_seconds(effectiveActiveDuration);
                room->BroadcastPacket(S_SkillEffectPacket(std::move(skillMsg)));
            }
        }
        else // ACTIVE
        {
            _dotTimer += dt;
            if (_dotTimer >= _dotInterval)
            {
                _dotTimer -= _dotInterval;

                float px = owner->GetX();
                float py = owner->GetY();
                float finalRadius = _hitRadius * effectiveAreaMult;
                int32_t finalDamage = static_cast<int32_t>(_damage * effectiveDamageMult);

                // Use GetMonstersInRange (Single Path)
                auto victims = room->GetMonstersInRange(px, py, finalRadius);
                std::vector<int32_t> hitTargetIds;
                std::vector<int32_t> hitDamageValues;

                const auto *tmpl = DataManager::Instance().GetSkillTemplate(_skillId);

                for (auto &monster : victims)
                {
                    monster->TakeDamage(finalDamage, room);
                    hitTargetIds.push_back(monster->GetId());
                    hitDamageValues.push_back(finalDamage);

                    if (tmpl && !tmpl->effectType.empty())
                    {
                        monster->AddStatusEffect(
                            tmpl->effectType, tmpl->effectValue, tmpl->effectDuration, room->GetTotalRunTime()
                        );
                    }
                }

                if (!hitTargetIds.empty())
                {
                    Protocol::S_DamageEffect damageMsg;
                    damageMsg.set_skill_id(_skillId);
                    for (int32_t tid : hitTargetIds)
                        damageMsg.add_target_ids(tid);
                    for (int32_t dmg : hitDamageValues)
                        damageMsg.add_damage_values(dmg);
                    room->BroadcastPacket(S_DamageEffectPacket(std::move(damageMsg)));
                }
            }

            if (_timer >= effectiveActiveDuration)
            {
                _state = EmitterState::COOLING;
                _timer = 0.0f;
            }
        }
        return;
    }

    // --- Standard Pulse/Projectile Logic (Original) ---
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
            // Use GetMonstersInRange (Single Path)
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
            std::vector<std::shared_ptr<Monster>> victims = room->GetMonstersInRange(px, py, finalRadius);

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

            // 1. Broadcast Visual Effect (Always fire visual for feedback)
            Protocol::S_SkillEffect skillMsg;
            skillMsg.set_caster_id(owner->GetId());
            skillMsg.set_skill_id(_skillId);
            skillMsg.set_x(px);
            skillMsg.set_y(py);
            skillMsg.set_radius(finalRadius);
            skillMsg.set_duration_seconds(currentTickInterval);
            for (int32_t tid : hitTargetIds)
                skillMsg.add_target_ids(tid);
            room->BroadcastPacket(S_SkillEffectPacket(std::move(skillMsg)));

            if (!hitTargetIds.empty())
            {
                // [DEBUG LOG]
                if (_skillId == 3) // Frost Nova ID
                {
                    LOG_INFO(
                        "[FrostNova] SkillId: {}, Targets: {}, Pos: ({:.2f}, {:.2f})",
                        _skillId,
                        hitTargetIds.size(),
                        px,
                        py
                    );
                }

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
