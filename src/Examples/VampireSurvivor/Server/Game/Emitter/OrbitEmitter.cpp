#include "Game/Emitter/OrbitEmitter.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Entity/ProjectileFactory.h"
#include "Game/DamageEmitter.h"
#include "Game/Room.h"
#include <cmath>

namespace SimpleGame {

OrbitEmitter::OrbitEmitter(float initialTimer) : _timer(initialTimer)
{
}

void OrbitEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
)
{
    _timer += dt;
    if (_timer >= stats.tickInterval)
    {
        _timer -= stats.tickInterval;

        float px = owner->GetX();
        float py = owner->GetY();
        int32_t projectileCount = stats.projectileCount;
        float orbitRadius = 3.0f * stats.areaMult;
        float orbitSpeed = 4.0f * stats.speedMult;
        float life = 4.0f * stats.durationMult;

        for (int i = 0; i < projectileCount; ++i)
        {
            float initialAngle = (static_cast<float>(i) / projectileCount) * 2.0f * 3.14159265f;
            float spawnX = px + orbitRadius * std::cos(initialAngle);
            float spawnY = py + orbitRadius * std::sin(initialAngle);

            auto proj = ProjectileFactory::Instance().CreateProjectile(
                room->GetObjectManager(),
                owner->GetId(),
                emitter->GetSkillId(),
                emitter->GetTypeId(),
                spawnX,
                spawnY,
                0, // velocity X
                0, // velocity Y
                stats.damage,
                life
            );

            if (proj)
            {
                proj->SetRadius(0.3f);
                proj->SetPierce(-1);
                proj->SetOrbit(orbitRadius, orbitSpeed, initialAngle);

                room->GetObjectManager().AddObject(proj);
                room->BroadcastSpawn({proj});
            }
        }
    }
}

} // namespace SimpleGame
