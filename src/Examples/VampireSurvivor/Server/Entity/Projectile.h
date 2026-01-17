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
        _vx = _vy = 0;
        _x = _y = 0;
        _isHit = false;
    }

    void Reset()
    {
        _id = 0;
        _ownerId = 0;
        _skillId = 0;
        _typeId = 0;
        _damage = 0;
        _lifetime = 0;
        _isHit = false;
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

    void Update(float dt, Room *room) override
    {
        // Movement is handled by Room::Update via GameObject::GetVX/VY
        // We only handle logic (lifetime)
        _lifetime -= dt;
    }

    bool IsExpired() const
    {
        return _lifetime <= 0 || _isHit;
    }
    void SetLifetime(float life)
    {
        _lifetime = life;
    }

    void SetHit()
    {
        _isHit = true;
    }
    bool IsHit() const
    {
        return _isHit;
    }

private:
    int32_t _ownerId = 0;
    int32_t _skillId = 0;
    int32_t _typeId = 0;
    int32_t _damage = 0;
    float _lifetime = 5.0f;
    bool _isHit = false;
};

} // namespace SimpleGame
