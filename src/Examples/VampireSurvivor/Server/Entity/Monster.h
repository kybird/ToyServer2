#pragma once
#include "AI/IAIBehavior.h"
#include "Entity/GameObject.h"
#include <memory>

namespace SimpleGame {

// Forward declaration
class IAIBehavior;

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

    // Called by Room::Update
    void Update(float dt, Room *room) override;

    void TakeDamage(int32_t damage, Room *room);
    bool IsDead() const { return _state == Protocol::ObjectState::DEAD; }

    // Pool reset
    void Reset()
    {
        _id = 0;
        _monsterTypeId = 0;
        _targetId = 0;
        _x = _y = _vx = _vy = 0;
        _hp = _maxHp = 100;
        _aliveTime = 0.0f;
        if (_ai)
            _ai->Reset();
    }

    void Initialize(int32_t id, int32_t monsterTypeId)
    {
        _id = id;
        _monsterTypeId = monsterTypeId;
        _aliveTime = 0.0f;
    }

private:
    int32_t _monsterTypeId = 0;
    int32_t _targetId = 0;
    float _aliveTime = 0.0f;
    std::unique_ptr<IAIBehavior> _ai;
};

} // namespace SimpleGame
