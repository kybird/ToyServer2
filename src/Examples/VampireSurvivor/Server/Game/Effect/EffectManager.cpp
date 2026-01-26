#include "EffectManager.h"
#include "Entity/GameObject.h"
#include "Game/Room.h"
#include "System/ILog.h"
#include <algorithm>

namespace SimpleGame {

void EffectManager::ApplyEffect(int32_t targetId, Effect::StatusEffect effect)
{
    auto &effects = _effects[targetId];

    // 같은 타입 효과가 있으면 갱신 (더 긴 시간으로)
    for (auto &existing : effects)
    {
        if (existing.type == effect.type)
        {
            if (effect.endTime > existing.endTime)
            {
                existing = effect;
                LOG_DEBUG("Effect {} refreshed on target {}", static_cast<int>(effect.type), targetId);
            }
            return;
        }
    }

    // 새 효과 추가
    effects.push_back(effect);
    LOG_DEBUG("Effect {} applied to target {}", static_cast<int>(effect.type), targetId);
}

void EffectManager::RemoveEffect(int32_t targetId, Effect::Type type)
{
    auto it = _effects.find(targetId);
    if (it == _effects.end())
        return;

    auto &effects = it->second;
    effects.erase(
        std::remove_if(
            effects.begin(),
            effects.end(),
            [type](const Effect::StatusEffect &e)
            {
                return e.type == type;
            }
        ),
        effects.end()
    );
}

void EffectManager::ClearEffects(int32_t targetId)
{
    _effects.erase(targetId);
}

void EffectManager::Update(float currentTime, Room *room)
{
    // [Crash Fix] Snapshot keys to handle map modification (add/remove keys) during iteration
    std::vector<int32_t> targets;
    targets.reserve(_effects.size());
    for (const auto &pair : _effects)
    {
        targets.push_back(pair.first);
    }

    for (int32_t targetId : targets)
    {
        auto it = _effects.find(targetId);
        if (it == _effects.end())
        {
            LOG_WARN("EffectManager::Update - Target {} removed during update, skipping", targetId);
            continue; // Removed during update
        }

        auto &effects = it->second;

        // [Crash Fix] Use index-based iteration
        for (size_t i = 0; i < effects.size(); ++i)
        {
            // [Safety] Re-fetch reference each time
            // Also, don't hold persistent reference if ProcessDotDamage can reallocate vector
            // But we must pass valid object.
            // If ApplyEffect is called on SAME target, vector reallocates.
            // So we must NOT pass 'effects[i]' by reference if the callee can add effects.
            // However, ProcessDotDamage just deals damage.
            // If TakeDamage triggers something... (e.g. Reflect Damage -> AddEffect)
            // It's safer to copy the Effect structure for processing (it's small)
            // or ensure we don't use the reference after the call if invalid.

            if (i >= effects.size())
            {
                LOG_ERROR("EffectManager::Update - Index {} out of bounds (size {}), breaking loop", i, effects.size());
                break; // Logic check
            }

            Effect::StatusEffect currentEffect = effects[i]; // Copy

            if (currentEffect.NeedsTick(currentTime))
            {
                // Pass copy to avoid dangling reference if vector reallocates
                ProcessDotDamage(targetId, currentEffect, currentTime, room);

                // [Fix] Re-verify existence. Since ProcessDotDamage might cause monster death
                // and ClearEffects(targetId), the entry in _effects might be gone.
                auto itCheck = _effects.find(targetId);
                if (itCheck == _effects.end())
                {
                    LOG_INFO(
                        "EffectManager::Update - Target {} died and effects cleared, breaking inner loop", targetId
                    );
                    break;
                }

                auto &currentEffects = itCheck->second;
                if (i < currentEffects.size())
                {
                    currentEffects[i].lastTickTime = currentTime;
                }
            }
        }
    }

    // 만료된 효과 정리
    CleanupExpired(currentTime);
}

void EffectManager::CleanupExpired(float currentTime)
{
    for (auto &[targetId, effects] : _effects)
    {
        effects.erase(
            std::remove_if(
                effects.begin(),
                effects.end(),
                [currentTime](const Effect::StatusEffect &e)
                {
                    return e.IsExpired(currentTime);
                }
            ),
            effects.end()
        );
    }

    // 빈 엔트리 제거
    for (auto it = _effects.begin(); it != _effects.end();)
    {
        if (it->second.empty())
            it = _effects.erase(it);
        else
            ++it;
    }
}

void EffectManager::ProcessDotDamage(int32_t targetId, Effect::StatusEffect &effect, float currentTime, Room *room)
{
    if (!room)
    {
        LOG_WARN("ProcessDotDamage - Room is null");
        return;
    }

    auto obj = room->GetObjectManager().GetObject(targetId);
    if (!obj)
    {
        LOG_WARN("ProcessDotDamage - Target {} not found in ObjectManager", targetId);
        return;
    }

    int32_t damage = static_cast<int32_t>(effect.value);
    obj->TakeDamage(damage, room);

    LOG_DEBUG("DoT {} dealt {} damage to target {}", static_cast<int>(effect.type), damage, targetId);
}

float EffectManager::GetSpeedMultiplier(int32_t targetId) const
{
    auto it = _effects.find(targetId);
    if (it == _effects.end())
        return 1.0f;

    float multiplier = 1.0f;
    for (const auto &effect : it->second)
    {
        if (effect.type == Effect::Type::SLOW)
            multiplier *= effect.value; // 0.5면 50% 속도
        else if (effect.type == Effect::Type::SPEED_BOOST)
            multiplier *= effect.value; // 1.5면 150% 속도
    }
    return multiplier;
}

float EffectManager::GetAttackMultiplier(int32_t targetId) const
{
    auto it = _effects.find(targetId);
    if (it == _effects.end())
        return 1.0f;

    float multiplier = 1.0f;
    for (const auto &effect : it->second)
    {
        if (effect.type == Effect::Type::ATTACK_UP)
            multiplier *= effect.value;
    }
    return multiplier;
}

bool EffectManager::HasEffect(int32_t targetId, Effect::Type type) const
{
    auto it = _effects.find(targetId);
    if (it == _effects.end())
        return false;

    for (const auto &effect : it->second)
    {
        if (effect.type == type)
            return true;
    }
    return false;
}

} // namespace SimpleGame
