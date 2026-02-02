#pragma once
#include "ModifierContainer.h"
#include "Protocol/game.pb.h"
#include <cstdint>
#include <memory>

namespace SimpleGame {

class Room; // Forward declaration

class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    GameObject(int32_t id, Protocol::ObjectType type) : _id(id), _type(type)
    {
    }
    virtual ~GameObject() = default;

    virtual void Update(float dt, Room *room)
    {
    } // AI/Logic Update

    virtual void TakeDamage(int32_t damage, Room *room)
    {
    }

    int32_t GetId() const
    {
        return _id;
    }
    Protocol::ObjectType GetType() const
    {
        return _type;
    }

    // Position & Movement
    float GetX() const
    {
        return _x;
    }
    float GetY() const
    {
        return _y;
    }
    void SetPos(float x, float y)
    {
        _x = x;
        _y = y;
    }

    float GetVX() const
    {
        return _vx;
    }
    float GetVY() const
    {
        return _vy;
    }
    void SetVelocity(float vx, float vy)
    {
        _vx = vx;
        _vy = vy;
    }

    float GetRadius() const
    {
        return _radius;
    }
    void SetRadius(float r)
    {
        _radius = r;
    }

    Protocol::ObjectState GetState() const
    {
        return _state;
    }
    void SetState(Protocol::ObjectState state, float expiresAt = 0.0f)
    {
        _state = state;
        _stateExpiresAt = expiresAt;
    }

    void UpdateStateExpiry(float currentTime);

    bool IsControlDisabled() const
    {
        return _state == Protocol::ObjectState::KNOCKBACK || _state == Protocol::ObjectState::STUNNED ||
               _state == Protocol::ObjectState::DEAD;
    }

    // Stats
    int32_t GetHp() const
    {
        return _hp;
    }
    void SetHp(int32_t hp)
    {
        _hp = hp;
    }
    int32_t GetMaxHp() const
    {
        return _maxHp;
    }

    bool IsDead() const
    {
        return _state == Protocol::ObjectState::DEAD;
    }

    // Network Sync State
    float GetLastSentVX() const
    {
        return _lastSentVX;
    }
    float GetLastSentVY() const
    {
        return _lastSentVY;
    }
    float GetLastSentX() const
    {
        return _lastSentX;
    }
    float GetLastSentY() const
    {
        return _lastSentY;
    }
    float GetLastSentTime() const
    {
        return _lastSentTime;
    }
    uint32_t GetLastSentServerTick() const
    {
        return _lastSentServerTick;
    }
    Protocol::ObjectState GetLastSentState() const
    {
        return _lastSentState;
    }

    void UpdateLastSentState(float time, uint32_t serverTick)
    {
        _lastSentVX = _vx;
        _lastSentVY = _vy;
        _lastSentX = _x;
        _lastSentY = _y;
        _lastSentTime = time;
        _lastSentServerTick = serverTick;
        _lastSentState = _state;
    }

    // Modifier Container 접근
    ModifierContainer &GetModifiers()
    {
        return _modifiers;
    }
    const ModifierContainer &GetModifiers() const
    {
        return _modifiers;
    }

protected:
    int32_t _id;
    Protocol::ObjectType _type;
    Protocol::ObjectState _state = Protocol::ObjectState::IDLE;
    float _stateExpiresAt = 0.0f;

    float _x = 0.0f;
    float _y = 0.0f;
    float _vx = 0.0f;
    float _vy = 0.0f;
    float _radius = 0.5f;

    int32_t _hp = 100;
    int32_t _maxHp = 100;

    float _lastSentX = 0.0f;
    float _lastSentY = 0.0f;
    float _lastSentVX = 0.0f;
    float _lastSentVY = 0.0f;

    // Default to IDLE (1) or NONE (0)?
    // Usually 0 is NONE, 1 is IDLE. Let's safe-guard with IDLE or ensure handled.
    // game.proto: IDLE = 1 (likely)
    float _lastSentTime = 0.0f;
    uint32_t _lastSentServerTick = 0;
    Protocol::ObjectState _lastSentState = Protocol::ObjectState::IDLE;

    // Stat Modifier System
    ModifierContainer _modifiers;

    // Spatial Grid Tracking
    long long _gridCellKey = 0;
    bool _isInGrid = false;

public:
    void SetGridInfo(long long key, bool inGrid)
    {
        _gridCellKey = key;
        _isInGrid = inGrid;
    }
    long long GetGridCellKey() const
    {
        return _gridCellKey;
    }
    bool IsInGrid() const
    {
        return _isInGrid;
    }
};

} // namespace SimpleGame
