#include "Game/DamageEmitter.h"
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

DamageEmitter::DamageEmitter(int32_t skillId, std::shared_ptr<Player> owner) : _skillId(skillId), _owner(owner)
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

    _timer += dt;
    if (_timer >= _tickInterval)
    {
        _timer -= _tickInterval;

        float px = owner->GetX();
        float py = owner->GetY();

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

            // Spawn slightly ahead of player
            float spawnOffset = owner->GetRadius() + 0.3f;
            float spawnX = px + direction.x * spawnOffset;
            float spawnY = py + direction.y * spawnOffset;

            auto proj = ProjectileFactory::Instance().CreateProjectile(
                room->_objMgr,
                owner->GetId(),
                _skillId,
                _typeId,
                spawnX,
                spawnY,
                direction.x * speed,
                direction.y * speed,
                _damage,
                life
            );

            if (proj)
            {
                proj->SetRadius(0.2f);
                room->_objMgr.AddObject(proj);
                room->_grid.Add(proj);
                room->BroadcastSpawn({proj});

                LOG_DEBUG(
                    "[DamageEmitter] Spawned Projectile ID={} TypeId={} at ({:.1f}, {:.1f}) Dir=({:.2f}, {:.2f})",
                    proj->GetId(),
                    _typeId,
                    spawnX,
                    spawnY,
                    direction.x,
                    direction.y
                );
            }
        }
        else
        {
            // AoE Damage logic
            auto victims = room->GetMonstersInRange(px, py, _hitRadius);
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
            for (auto &monster : victims)
            {
                if (count >= _maxTargetsPerTick)
                    break;
                monster->TakeDamage(_damage, room);
                count++;
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
