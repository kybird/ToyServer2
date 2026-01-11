#include "Game/DamageEmitter.h"
#include "Core/DataManager.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Game/Room.h"
#include "System/ILog.h"
#include <algorithm>

namespace SimpleGame {

DamageEmitter::DamageEmitter(int32_t skillId, std::shared_ptr<Player> owner) : _skillId(skillId), _owner(owner)
{
    const auto *tmpl = DataManager::Instance().GetSkillTemplate(skillId);
    if (tmpl)
    {
        _damage = tmpl->damage;
        _tickInterval = tmpl->tickInterval;
        _hitRadius = tmpl->hitRadius;
        _emitterType = tmpl->emitterType;
        _pierce = tmpl->pierce;
        _maxTargetsPerTick = tmpl->maxTargetsPerTick;
        _targetRule = tmpl->targetRule;
        _lifeTime = tmpl->lifeTime;
    }
    // Start ready to fire
    _timer = _tickInterval;
}

void DamageEmitter::Update(float dt, Room *room)
{
    if (!_owner || _owner->IsDead() || !room || !_active)
        return;

    _elapsedTime += dt;
    if (_lifeTime > 0.0f && _elapsedTime >= _lifeTime)
    {
        _active = false;
        return;
    }

    _timer += dt;
    if (_timer >= _tickInterval)
    {
        _timer -= _tickInterval;

        float px = _owner->GetX();
        float py = _owner->GetY();

        std::vector<std::shared_ptr<Monster>> victims;

        if (_emitterType == "Linear")
        {
            // 플레이어의 앞 방향 0.5m ~ hitRadius 거리 내의 몬스터 선정 (간소화된 판정)
            // 실제로는 플레이어 위치에서 방향 벡터로 hitRadius만큼 떨어진 점을 중심으로 radius 판정
            Vector2 forward = _owner->GetFacingDirection();
            float targetX = px + forward.x * (_hitRadius * 0.5f);
            float targetY = py + forward.y * (_hitRadius * 0.5f);

            victims = room->GetMonstersInRange(targetX, targetY, _hitRadius * 0.5f);
        }
        else
        {
            // 기본 AoE (Circular)
            victims = room->GetMonstersInRange(px, py, _hitRadius);
        }

        // 타겟 규칙 적용 (Nearest)
        if (_targetRule == "Nearest")
        {
            std::sort(
                victims.begin(),
                victims.end(),
                [&](const auto &a, const auto &b)
                {
                    float distA = Vector2::DistanceSq(Vector2(px, py), Vector2(a->GetX(), a->GetY()));
                    float distB = Vector2::DistanceSq(Vector2(px, py), Vector2(b->GetX(), b->GetY()));
                    return distA < distB;
                }
            );
        }

        // 최대 타겟 수 제한
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

bool DamageEmitter::IsExpired() const
{
    if (!_active)
        return true;
    if (_owner && _owner->IsDead())
        return true;
    return false;
}

} // namespace SimpleGame
