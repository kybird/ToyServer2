#include "Entity/Projectile.h"
#include "Game/Room.h"
#include <cmath>

namespace SimpleGame {

void Projectile::Update(float dt, Room *room)
{
    if (IsDead())
        return;

    if (_isOrbit)
    {
        UpdateOrbit(dt, room);
    }
    else
    {
        UpdateLinear(dt, room);
    }

    // Lifetime check
    _lifetime -= dt;
    if (_lifetime <= 0 || _isHit)
    {
        SetState(Protocol::ObjectState::DEAD);
    }
}

void Projectile::UpdateLinear(float dt, Room *room)
{
    // Track traveled distance
    float speed = std::sqrt(_vx * _vx + _vy * _vy);
    _traveledDistance += speed * dt;

    // Check max range
    if (_traveledDistance >= _maxRange)
    {
        SetState(Protocol::ObjectState::DEAD);
        return;
    }

    // Position update
    _x += _vx * dt;
    _y += _vy * dt;
}

void Projectile::UpdateOrbit(float dt, Room *room)
{
    auto owner = room->GetObjectManager().GetObject(_ownerId);
    if (!owner || owner->IsDead())
    {
        SetState(Protocol::ObjectState::DEAD);
        return;
    }

    _currentAngle += _orbitSpeed * dt;

    // Owner 위치 + 반경 * 방향(sin, cos)
    float targetX = owner->GetX() + _orbitRadius * std::cos(_currentAngle);
    float targetY = owner->GetY() + _orbitRadius * std::sin(_currentAngle);

    // 부드러운 이동을 위해 속도를 계산하여 설정 (클라이언트 DeadReckoning 지원용)
    float vx = -_orbitRadius * std::sin(_currentAngle) * _orbitSpeed;
    float vy = _orbitRadius * std::cos(_currentAngle) * _orbitSpeed;

    SetPos(targetX, targetY);
    SetVelocity(vx, vy);
}

} // namespace SimpleGame
