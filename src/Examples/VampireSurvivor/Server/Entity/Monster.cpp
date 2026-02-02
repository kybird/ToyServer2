#include "Monster.h"
#include "AI/IAIBehavior.h"
#include "Entity/AI/Movement/IMovementStrategy.h"
#include "Entity/AI/Movement/SmartFlockingStrategy.h"
#include "Game/Room.h"

namespace SimpleGame {

Monster::Monster(int32_t id, int32_t monsterTypeId)
    : GameObject(id, Protocol::ObjectType::MONSTER), _monsterTypeId(monsterTypeId)
{
    _movementStrategy = std::make_shared<SmartFlockingStrategy>();
}

Monster::Monster() : GameObject(0, Protocol::ObjectType::MONSTER)
{
    _movementStrategy = std::make_shared<SmartFlockingStrategy>();
}

void Monster::SetMovementStrategy(std::shared_ptr<IMovementStrategy> strategy)
{
    _movementStrategy = strategy;
}

std::shared_ptr<IMovementStrategy> Monster::GetMovementStrategy()
{
    return _movementStrategy;
}

constexpr int32_t LEVELUP_SLOW_SOURCE_ID = 1000;

void Monster::Update(float dt, Room *room)
{
    _aliveTime += dt;
    if (_stuckTimer > 0.0f)
    {
        _stuckTimer -= dt;
    }

    UpdateStateExpiry(room->GetTotalRunTime());
    _modifiers.Update(room->GetTotalRunTime());

    if (_ai != nullptr && !IsDead())
    {
        if (IsControlDisabled())
            return;
        _ai->Think(this, room, _aliveTime);
        _ai->Execute(this, dt);
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
    StatModifier slowMod(
        StatType::Speed, ModifierOp::PercentMult, 0.15f, LEVELUP_SLOW_SOURCE_ID, currentTime + duration, false
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
        StatModifier mod(StatType::Speed, ModifierOp::PercentMult, value, 2000, currentTime + duration);
        _modifiers.AddModifier(mod);
    }
}

void Monster::Reset()
{
    _id = 0;
    _monsterTypeId = 0;
    _targetId = 0;
    _x = _y = _vx = _vy = 0;
    _hp = _maxHp = 100;
    _aliveTime = 0.0f;
    _radius = GameConfig::MONSTER_COLLISION_RADIUS;
    _damageOnContact = 10;
    _attackCooldown = 1.0f;
    _lastAttackTime = -100.0f;
    _state = Protocol::ObjectState::IDLE;
    _stateExpiresAt = 0.0f;
    if (_ai)
        _ai->Reset();
    _modifiers.Clear();
}

void Monster::Initialize(
    int32_t id, int32_t monsterTypeId, int32_t hp, float radius, int32_t damage, float cooldown, float speed
)
{
    _id = id;
    _monsterTypeId = monsterTypeId;
    _hp = _maxHp = hp;
    _radius = GameConfig::MONSTER_COLLISION_RADIUS;
    _damageOnContact = damage;
    _attackCooldown = cooldown;
    _lastAttackTime = -100.0f;
    _aliveTime = 0.0f;
    _state = Protocol::ObjectState::IDLE;
    _modifiers.SetBaseStat(StatType::Speed, speed);
}

} // namespace SimpleGame
