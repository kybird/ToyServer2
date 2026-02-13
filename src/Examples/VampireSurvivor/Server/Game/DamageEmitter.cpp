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
#include "System/Utility/FastRandom.h"
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
        _arcDegrees = tmpl->arcDegrees; // [New] Load arc angle
        _width = tmpl->width;           // [New] Initial width
        _height = tmpl->height;         // [New] Initial height

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
    const auto *tmpl = DataManager::Instance().GetSkillTemplate(_skillId);
    auto hasTrait = [&](const std::string &trait)
    {
        if (!tmpl)
            return false;
        return std::find(tmpl->traits.begin(), tmpl->traits.end(), trait) != tmpl->traits.end();
    };

    float effectiveDamageMult = owner->GetDamageMultiplier();
    float effectiveCooldownMult = owner->GetCooldownMultiplier();
    float effectiveAreaMult = 1.0f;
    float effectiveDurationMult = 1.0f;
    int32_t additionalProjectiles = 0;
    int32_t additionalPierce = 0;

    if (hasTrait("AREA") || hasTrait("AOE"))
        effectiveAreaMult = owner->GetAreaMultiplier();

    if (hasTrait("DURATION"))
        effectiveDurationMult = owner->GetDurationMultiplier();

    if (hasTrait("PROJECTILE"))
    {
        additionalProjectiles = owner->GetAdditionalProjectileCount();
        additionalPierce = owner->GetAdditionalPierceCount();
    }

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

            // [New] Apply generic parameter overrides (e.g., width_mult, height_mult)
            if (levelData.params.contains("skill_width_mult"))
                effectiveAreaMult *= levelData.params.at("skill_width_mult"); // Simple approach
            if (levelData.params.contains("skill_height_mult"))
            {
                // For Directional, we might want separate multipliers
            }
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

    // --- Pulse/Projectile/Special Logic ---
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
            auto monsters = room->GetMonstersInRange(px, py, 30.0f);
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
                        direction.Normalize();
                    else
                        direction = owner->GetFacingDirection();
                }
            }

            float speed = 15.0f;
            float life = 3.0f;
            int32_t projectileCount = 1 + additionalProjectiles;

            for (int i = 0; i < projectileCount; ++i)
            {
                Vector2 fireDir = direction;
                if (projectileCount > 1)
                {
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
                    proj->SetPierce(_pierce + additionalPierce);
                    room->_objMgr.AddObject(proj);
                    room->_grid.Add(proj);
                    room->BroadcastSpawn({proj});
                }
            }
        }
        else if (_emitterType == "Orbit")
        {
            // [Orbit] 성서형: 플레이어 주변을 공전하는 투사체
            int32_t projectileCount = 1 + additionalProjectiles;
            float orbitRadius = 3.0f * effectiveAreaMult;
            float orbitSpeed = 4.0f; // 매초 약 0.6회전
            float life = 4.0f * effectiveDurationMult;

            for (int i = 0; i < projectileCount; ++i)
            {
                // 초기 각도 설정 (균등 분배)
                float initialAngle = (static_cast<float>(i) / projectileCount) * 2.0f * 3.14159265f;

                // 투사체 위치 계산 (초기 위치)
                float spawnX = px + orbitRadius * std::cos(initialAngle);
                float spawnY = py + orbitRadius * std::sin(initialAngle);

                auto proj = ProjectileFactory::Instance().CreateProjectile(
                    room->_objMgr, owner->GetId(), _skillId, _typeId, spawnX, spawnY, 0, 0, finalDamage, life
                );

                if (proj)
                {
                    proj->SetRadius(0.3f);
                    proj->SetPierce(-1); // 무한 관통
                    proj->SetOrbit(orbitRadius, orbitSpeed, initialAngle);

                    room->_objMgr.AddObject(proj);
                    room->BroadcastSpawn({proj});

                    LOG_INFO(
                        "[DamageEmitter] Spawned Orbit Projectile: ID={}, Owner={}, Angle={:.2f}",
                        proj->GetId(),
                        owner->GetId(),
                        initialAngle
                    );
                }
            }
        }
        else if (_emitterType == "Zone")
        {
            // [Zone] 번개형: 화면 내 랜덤 적 즉시 타격
            auto monsters = room->GetMonstersInRange(px, py, 20.0f); // 화면 근처 범위
            if (!monsters.empty())
            {
                int32_t shotCount = 1 + additionalProjectiles;
                std::vector<int32_t> hitIds;
                std::vector<int32_t> hitDamages;

                std::shuffle(monsters.begin(), monsters.end(), std::mt19937(std::random_device()()));

                for (int i = 0; i < std::min(shotCount, (int32_t)monsters.size()); ++i)
                {
                    auto m = monsters[i];
                    m->TakeDamage(finalDamage, room);
                    hitIds.push_back(m->GetId());
                    hitDamages.push_back(finalDamage);

                    // 각 타격 위치에 이펙트 전송
                    Protocol::S_SkillEffect eff;
                    eff.set_caster_id(owner->GetId());
                    eff.set_skill_id(_skillId);
                    eff.set_x(m->GetX());
                    eff.set_y(m->GetY());
                    eff.set_radius(1.0f);
                    eff.set_duration_seconds(0.2f);
                    room->BroadcastPacket(S_SkillEffectPacket(std::move(eff)));
                }

                if (!hitIds.empty())
                {
                    Protocol::S_DamageEffect dmgMsg;
                    dmgMsg.set_skill_id(_skillId);
                    for (int id : hitIds)
                        dmgMsg.add_target_ids(id);
                    for (int d : hitDamages)
                        dmgMsg.add_damage_values(d);
                    room->BroadcastPacket(S_DamageEffectPacket(std::move(dmgMsg)));
                }
            }
        }
        else if (_emitterType == "Directional")
        {
            // [Directional] 채찍형: 바라보는 방향 직사각형 판정
            Vector2 dir = owner->GetFacingDirection();

            // Load from Level Data overrides if present
            float widthMult = 1.0f;
            float heightMult = 1.0f;
            if (_weaponId > 0)
            {
                const auto *weaponTmpl = DataManager::Instance().GetWeaponTemplate(_weaponId);
                if (weaponTmpl != nullptr && _level > 0 && static_cast<size_t>(_level) <= weaponTmpl->levels.size())
                {
                    const auto &levelData = weaponTmpl->levels[static_cast<size_t>(_level) - 1];
                    if (levelData.params.contains("skill_width_mult"))
                        widthMult = levelData.params.at("skill_width_mult");
                    if (levelData.params.contains("skill_height_mult"))
                        heightMult = levelData.params.at("skill_height_mult");
                }
            }

            // [Data-Driven] Use _width and _height instead of hardcoded 2.0/4.0
            float finalBoxWidth = _width * effectiveAreaMult * widthMult;
            float finalBoxHeight = _height * effectiveAreaMult * heightMult;

            // 판정 중심점 (플레이어 정면)
            float cx = px + dir.x * (finalBoxHeight * 0.5f);
            float cy = py + dir.y * (finalBoxHeight * 0.5f);

            // 각도 계산 (라디안)
            float angle = std::atan2(dir.y, dir.x);
            float sinA = std::sin(-angle);
            float cosA = std::cos(-angle);

            auto monsters = room->GetMonstersInRange(cx, cy, finalBoxHeight);
            std::vector<int32_t> hitIds;
            std::vector<int32_t> hitDamages;
            std::vector<bool> hitCrits;

            // 크리티컬 체크
            bool isCritical = false;
            float critMultiplier = 1.0f;
            if (System::Utility::FastRandom().NextFloat() < owner->GetCriticalChance())
            {
                isCritical = true;
                critMultiplier = owner->GetCriticalDamageMultiplier();
            }
            int32_t finalCritDamage = static_cast<int32_t>(finalDamage * critMultiplier);

            for (auto &m : monsters)
            {
                // 상대 좌표 변환 (Rotate toward 0)
                float dx = m->GetX() - cx;
                float dy = m->GetY() - cy;

                float localX = dx * cosA - dy * sinA;
                float localY = dx * sinA + dy * cosA;

                // 직사각형 판정 (finalBoxHeight는 플레이어 정면 방향 길이, finalBoxWidth는 좌우 너비)
                if (std::abs(localX) <= finalBoxHeight * 0.5f && std::abs(localY) <= finalBoxWidth * 0.5f)
                {
                    m->TakeDamage(finalCritDamage, room);
                    hitIds.push_back(m->GetId());
                    hitDamages.push_back(finalCritDamage);
                    hitCrits.push_back(isCritical);
                }
            }

            if (!hitIds.empty())
            {
                Protocol::S_DamageEffect dmgMsg;
                dmgMsg.set_skill_id(_skillId);
                for (size_t i = 0; i < hitIds.size(); ++i)
                {
                    dmgMsg.add_target_ids(hitIds[i]);
                    dmgMsg.add_damage_values(hitDamages[i]);
                    dmgMsg.add_is_critical(hitCrits[i]);
                }
                room->BroadcastPacket(S_DamageEffectPacket(std::move(dmgMsg)));
            }

            // 시각 효과
            Protocol::S_SkillEffect skillMsg;
            skillMsg.set_caster_id(owner->GetId());
            skillMsg.set_skill_id(_skillId);
            skillMsg.set_x(cx);
            skillMsg.set_y(cy);
            skillMsg.set_radius(finalBoxHeight * 0.5f); // 비주얼용 가이드 반경
            skillMsg.set_duration_seconds(0.2f);
            skillMsg.set_width(finalBoxWidth);   // [New] Pass dynamic width
            skillMsg.set_height(finalBoxHeight); // [New] Pass dynamic height

            float rotDeg = angle * (180.0f / 3.14159265f);
            skillMsg.set_rotation_degrees(rotDeg);

            room->BroadcastPacket(S_SkillEffectPacket(std::move(skillMsg)));
        }
        else if (_emitterType == "Arc")
        {
            Vector2 direction = owner->GetFacingDirection();
            auto monsters = room->GetMonstersInRange(px, py, finalRadius);

            std::vector<int32_t> hitTargetIds;
            std::vector<int32_t> hitDamageValues;
            std::vector<bool> hitCriticals;

            bool isCritical = false;
            float critMultiplier = 1.0f;
            float critChance = owner->GetCriticalChance();

            static thread_local System::Utility::FastRandom rng;
            if (rng.NextFloat() < critChance)
            {
                isCritical = true;
                critMultiplier = owner->GetCriticalDamageMultiplier();
            }

            int32_t criticalDamage = static_cast<int32_t>(finalDamage * critMultiplier);

            for (auto &monster : monsters)
            {
                if (monster->IsDead())
                    continue;

                Vector2 toMonster(monster->GetX() - px, monster->GetY() - py);
                float distSq = toMonster.MagnitudeSq();

                if (distSq > finalRadius * finalRadius)
                    continue;

                toMonster.Normalize();

                float dot = direction.x * toMonster.x + direction.y * toMonster.y;
                float angleRad = std::acos(std::clamp(dot, -1.0f, 1.0f));
                float angleDeg = angleRad * 180.0f / 3.14159265f;

                if (angleDeg <= _arcDegrees / 2.0f)
                {
                    monster->TakeDamage(criticalDamage, room);
                    hitTargetIds.push_back(monster->GetId());
                    hitDamageValues.push_back(criticalDamage);
                    hitCriticals.push_back(isCritical);
                }
            }

            if (!hitTargetIds.empty())
            {
                Protocol::S_DamageEffect damageMsg;
                damageMsg.set_skill_id(_skillId);
                for (size_t i = 0; i < hitTargetIds.size(); ++i)
                {
                    damageMsg.add_target_ids(hitTargetIds[i]);
                    damageMsg.add_damage_values(hitDamageValues[i]);
                    damageMsg.add_is_critical(hitCriticals[i]);
                }
                room->BroadcastPacket(S_DamageEffectPacket(std::move(damageMsg)));
            }

            Protocol::S_SkillEffect skillMsg;
            skillMsg.set_caster_id(owner->GetId());
            skillMsg.set_skill_id(_skillId);
            skillMsg.set_x(px);
            skillMsg.set_y(py);
            skillMsg.set_radius(finalRadius);
            skillMsg.set_duration_seconds(0.3f);
            skillMsg.set_arc_degrees(_arcDegrees);

            float rotDeg = std::atan2(direction.y, direction.x) * (180.0f / 3.14159265f);
            skillMsg.set_rotation_degrees(rotDeg);

            room->BroadcastPacket(S_SkillEffectPacket(std::move(skillMsg)));
        }
        else
        {
            // AoE Pulse Damage (Aura/Garlic) - _activeDuration == 0인 경우의 기본 처리
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
            std::vector<bool> hitCrits;

            // 크리티컬 체크
            bool isCritical = false;
            float critMultiplier = 1.0f;
            if (System::Utility::FastRandom().NextFloat() < owner->GetCriticalChance())
            {
                isCritical = true;
                critMultiplier = owner->GetCriticalDamageMultiplier();
            }
            int32_t finalCritDamage = static_cast<int32_t>(finalDamage * critMultiplier);

            for (auto &monster : victims)
            {
                if (count >= finalMaxTargets)
                    break;

                monster->TakeDamage(finalCritDamage, room);
                hitTargetIds.push_back(monster->GetId());
                hitDamageValues.push_back(finalCritDamage);
                hitCrits.push_back(isCritical);
                count++;
            }

            if (!hitTargetIds.empty())
            {
                LOG_INFO(
                    "[DamageEmitter] AoE Pulse Hit: Skill={}, Targets={}, Damage={}",
                    _skillId,
                    hitTargetIds.size(),
                    finalCritDamage
                );

                Protocol::S_DamageEffect damageMsg;
                damageMsg.set_skill_id(_skillId);
                for (size_t i = 0; i < hitTargetIds.size(); ++i)
                {
                    damageMsg.add_target_ids(hitTargetIds[i]);
                    damageMsg.add_damage_values(hitDamageValues[i]);
                    damageMsg.add_is_critical(hitCrits[i]);
                }
                room->BroadcastPacket(S_DamageEffectPacket(std::move(damageMsg)));
            }
            else
            {
                static float lastNoTargetLogTime = 0.0f;
                if (_elapsedTime - lastNoTargetLogTime > 1.0f)
                {
                    lastNoTargetLogTime = _elapsedTime;
                    LOG_DEBUG(
                        "[DamageEmitter] AoE Pulse No Target: Skill={}, Radius={:.2f}, VictimsInRange={}",
                        _skillId,
                        finalRadius,
                        victims.size()
                    );
                }
            }

            Protocol::S_SkillEffect skillMsg;
            skillMsg.set_caster_id(owner->GetId());
            skillMsg.set_skill_id(_skillId);
            skillMsg.set_x(px);
            skillMsg.set_y(py);
            skillMsg.set_radius(finalRadius);
            skillMsg.set_duration_seconds(0.2f);
            room->BroadcastPacket(S_SkillEffectPacket(std::move(skillMsg)));
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
