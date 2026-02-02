#pragma once
#include "AI/IAIBehavior.h"
#include "Entity/GameObject.h"
#include "Game/GameConfig.h"
#include <memory>

namespace SimpleGame {

class IAIBehavior;
class Room;
class IMovementStrategy;

class Monster : public GameObject
{
public:
    Monster(int32_t id, int32_t monsterTypeId);
    Monster();
    virtual ~Monster() = default;

    int32_t GetMonsterTypeId() const
    {
        return _monsterTypeId;
    }
    void SetMonsterTypeId(int32_t typeId)
    {
        _monsterTypeId = typeId;
    }

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

    void AddLevelUpSlow(float currentTime, float duration);
    void RemoveLevelUpSlow();
    void AddStatusEffect(const std::string &type, float value, float duration, float currentTime);

    void SetMovementStrategy(std::shared_ptr<IMovementStrategy> strategy);
    std::shared_ptr<IMovementStrategy> GetMovementStrategy();

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

    void Reset();
    void Initialize(
        int32_t id, int32_t monsterTypeId, int32_t hp, float radius, int32_t damage, float cooldown, float speed
    );

    float GetSpeed()
    {
        return _modifiers.GetStat(StatType::Speed);
    }

    void SetStuckTimer(float time)
    {
        _stuckTimer = time;
    }
    float GetStuckTimer() const
    {
        return _stuckTimer;
    }

private:
    std::unique_ptr<IAIBehavior> _ai;
    std::shared_ptr<IMovementStrategy> _movementStrategy;

    int32_t _monsterTypeId = 0;
    int32_t _targetId = 0;
    float _aliveTime = 0.0f;
    float _stuckTimer = 0.0f;

    int32_t _damageOnContact = 10;
    float _attackCooldown = 1.0f;
    float _lastAttackTime = -100.0f;
};
} // namespace SimpleGame
