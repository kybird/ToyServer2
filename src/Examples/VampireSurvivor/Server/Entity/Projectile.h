#pragma once
#include "Entity/GameObject.h"

namespace SimpleGame {

class Projectile : public GameObject
{
public:
    Projectile(int32_t id, int32_t ownerId, int32_t skillId)
        : GameObject(id, Protocol::ObjectType::PROJECTILE), _ownerId(ownerId), _skillId(skillId)
    {
    }

    // Default constructor for pooling
    Projectile() : GameObject(0, Protocol::ObjectType::PROJECTILE)
    {
    }

    void Initialize(int32_t id, int32_t ownerId, int32_t skillId, int32_t typeId)
    {
        _id = id;
        _ownerId = ownerId;
        _skillId = skillId;
        _typeId = typeId;
        _damage = 0;
        _lifetime = 5.0f;
        _maxRange = 50.0f; // Default max range
        _traveledDistance = 0.0f;
        _vx = 0.0f;
        _vy = 0.0f;
        _x = 0.0f;
        _y = 0.0f;
        _isHit = false;
        SetState(Protocol::ObjectState::IDLE); // [Fix] Reset state for pooled object
    }

    void Reset()
    {
        _id = 0;
        _ownerId = 0;
        _skillId = 0;
        _typeId = 0;
        _damage = 0;
        _lifetime = 0.0f;
        _maxRange = 50.0f;
        _traveledDistance = 0.0f;
        _vx = 0.0f;
        _vy = 0.0f;
        _isHit = false;
        SetState(Protocol::ObjectState::IDLE);
    }

    int32_t GetOwnerId() const
    {
        return _ownerId;
    }
    int32_t GetSkillId() const
    {
        return _skillId;
    }
    int32_t GetTypeId() const
    {
        return _typeId;
    }

    void SetDamage(int32_t dmg)
    {
        _damage = dmg;
    }
    int32_t GetDamage() const
    {
        return _damage;
    }

    void Update(float dt, Room *room) override;

    void UpdateLinear(float dt, Room *room);

    void UpdateOrbit(float dt, Room *room);

    bool IsExpired() const
    {
        return _lifetime <= 0 || _isHit || IsDead();
    }
    void SetLifetime(float life)
    {
        _lifetime = life;
    }

    void SetPierce(int32_t count)
    {
        _pierceCount = count;
    }

    // Orbit 설정
    void SetOrbit(float radius, float speed, float initialAngle)
    {
        _isOrbit = true;
        _orbitRadius = radius;
        _orbitSpeed = speed;
        _currentAngle = initialAngle;
    }

    // Returns true if projectile is consumed (expired)
    bool OnHit()
    {
        if (_pierceCount > 0)
        {
            _pierceCount--;
            return false; // Not expired yet (Pierced)
        }
        else
        {
            _isHit = true;
            return true; // Expired
        }
    }

    // Deprecated: Use OnHit logic instead or keep for backward compatibility checking IsHit
    void SetHit()
    {
        OnHit();
    }

    bool IsHit() const
    {
        return _isHit;
    }

    void SetMaxRange(float range)
    {
        _maxRange = range;
    }

    float GetTraveledDistance() const
    {
        return _traveledDistance;
    }

    int32_t GetPierceCount() const
    {
        return _pierceCount;
    }

private:
    int32_t _ownerId = 0;
    int32_t _skillId = 0;
    int32_t _typeId = 0;
    int32_t _damage = 0;
    float _lifetime = 5.0f;
    float _maxRange = 50.0f;        // [New] Maximum travel distance
    float _traveledDistance = 0.0f; // [New] Accumulated travel distance
    bool _isHit = false;
    int32_t _pierceCount = 0;

    // Orbit members
    bool _isOrbit = false;
    float _orbitRadius = 0.0f;
    float _orbitSpeed = 0.0f;
    float _currentAngle = 0.0f;
};

} // namespace SimpleGame
