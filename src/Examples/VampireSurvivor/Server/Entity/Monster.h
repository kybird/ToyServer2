#pragma once
#include "AI/IAIBehavior.h"
#include "Entity/GameObject.h"
#include "Game/GameConfig.h"
#include <memory>

namespace SimpleGame {

// Forward declaration
class IAIBehavior;
class Room;

/**
 * @brief Monster entity with pluggable AI behavior.
 */
class Monster : public GameObject
{
public:
    Monster(int32_t id, int32_t monsterTypeId)
        : GameObject(id, Protocol::ObjectType::MONSTER), _monsterTypeId(monsterTypeId)
    {
    }

    // Default constructor for pooling
    Monster() : GameObject(0, Protocol::ObjectType::MONSTER)
    {
    }

    int32_t GetMonsterTypeId() const
    {
        return _monsterTypeId;
    }
    void SetMonsterTypeId(int32_t typeId)
    {
        _monsterTypeId = typeId;
    }

    // AI Management
    void SetAI(std::unique_ptr<IAIBehavior> ai)
    {
        _ai = std::move(ai);
    }
    IAIBehavior *GetAI() const
    {
        return _ai.get();
    }

    void SetTargetId(int32_t targetId)
    {
        _targetId = targetId;
    }
    int32_t GetTargetId() const
    {
        return _targetId;
    }

    // Combat Stats
    bool CanAttack(float currentTime) const
    {
        return (currentTime - _lastAttackTime) >= _attackCooldown;
    }
    void ResetAttackCooldown(float currentTime)
    {
        _lastAttackTime = currentTime;
    }
    int32_t GetContactDamage() const
    {
        return _damageOnContact;
    }

    // Speed Control (via Modifier System)
    void AddLevelUpSlow(float currentTime, float duration);
    void RemoveLevelUpSlow();
    void AddStatusEffect(const std::string &type, float value, float duration, float currentTime);

    // Called by Room::Update
    void Update(float dt, Room *room) override;

    void TakeDamage(int32_t damage, Room *room) override;
    bool IsDead() const
    {
        return _state == Protocol::ObjectState::DEAD;
    }

    float GetAliveTime() const
    {
        return _aliveTime;
    }

    // Pool reset
    void Reset()
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

    void
    Initialize(int32_t id, int32_t monsterTypeId, int32_t hp, float radius, int32_t damage, float cooldown, float speed)
    {
        _id = id;
        _monsterTypeId = monsterTypeId;
        _hp = _maxHp = hp;
        _radius = GameConfig::MONSTER_COLLISION_RADIUS; // Lag Compensation for Body Attack
        (void)radius;                                   // Suppress unused warning
        _damageOnContact = damage;
        _attackCooldown = cooldown;
        _lastAttackTime = -100.0f;
        _aliveTime = 0.0f;
        _state = Protocol::ObjectState::IDLE;

        // Set base speed via Modifier System
        _modifiers.SetBaseStat(StatType::Speed, speed);
    }

    float GetSpeed()
    {
        return _modifiers.GetStat(StatType::Speed);
    }

private:
    // Pointers (8 bytes)
    std::unique_ptr<IAIBehavior> _ai;

    // 4 bytes members
    int32_t _monsterTypeId = 0;
    int32_t _targetId = 0;
    float _aliveTime = 0.0f;

    // Extended Combat Stats
    int32_t _damageOnContact = 10;
    float _attackCooldown = 1.0f;
    float _lastAttackTime = -100.0f;

    // Control Stats (removed - now using ModifierContainer)
};

} // namespace SimpleGame
