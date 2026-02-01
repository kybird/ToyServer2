#include "Monster.h"
#include "AI/IAIBehavior.h"
#include "Game/Room.h"

namespace SimpleGame {

// LevelUp Slow Source ID (고유 식별자)
constexpr int32_t LEVELUP_SLOW_SOURCE_ID = 1000;

void Monster::Update(float dt, Room *room)
{
    // Update lifetime
    _aliveTime += dt;

    // Update state expiry
    // [Fix] Use room->GetTotalRunTime() because state expiration is set based on room time
    UpdateStateExpiry(room->GetTotalRunTime());

    // Update Modifier expiration
    _modifiers.Update(room->GetTotalRunTime());

    if (_ai != nullptr && !IsDead())
    {
        // Skip AI if control is disabled (e.g. Knockback, Stun)
        if (IsControlDisabled())
        {
            return;
        }

        // Create AI based on template
        _ai->Think(this, room, _aliveTime);
        _ai->Execute(this, dt);

        /*
        // --- 중앙 집중식 회피 및 스티어링 (매 프레임 처리) ---
        // [Optimization] Individual query is removed. Cell-centric loop in Room::Update handles this now.
        Vector2 velocity(GetVX(), GetVY());
        Vector2 sep = room->GetSeparationVector(GetX(), GetY(), GetRadius() * 2.5f, GetId(), velocity, 6);

        if (!sep.IsZero())
        {
            Vector2 n = sep.Normalized();
            float proj = velocity.Dot(n);

            if (proj < 0.0f)
            {
                velocity -= n * proj;
                SetVelocity(velocity.x, velocity.y);
            }
        }
        */
    }
}

void Monster::TakeDamage(int32_t damage, Room *room)
{
    if (IsDead())
        return;

    _hp -= damage;
    if (_hp <= 0)
    {
        _hp = 0;
        SetState(Protocol::ObjectState::DEAD);
    }
}

void Monster::AddLevelUpSlow(float currentTime, float duration)
{
    // 15% 속도로 감소 (PercentMult 0.15)
    StatModifier slowMod(
        StatType::Speed,
        ModifierOp::PercentMult,
        0.15f, // 15% of normal speed
        LEVELUP_SLOW_SOURCE_ID,
        currentTime + duration, // 만료 시각
        false                   // Refresh 정책
    );

    _modifiers.AddModifier(slowMod);
}

void Monster::RemoveLevelUpSlow()
{
    _modifiers.RemoveBySourceId(LEVELUP_SLOW_SOURCE_ID);
}

void Monster::AddStatusEffect(const std::string &type, float value, float duration, float currentTime)
{
    if (type == "SLOW")
    {
        // sourceId 2000+ for general status effects to avoid conflict with levelup slow(1000)
        StatModifier mod(StatType::Speed, ModifierOp::PercentMult, value, 2000, currentTime + duration);
        _modifiers.AddModifier(mod);
    }
}

} // namespace SimpleGame
