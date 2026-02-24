#include "Game/Emitter/LinearEmitter.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Entity/ProjectileFactory.h"
#include "Game/DamageEmitter.h"
#include "Game/Room.h"
#include "Math/Vector2.h"
#include <cmath>
#include <limits>

namespace SimpleGame {

LinearEmitter::LinearEmitter(float initialTimer) : _timer(initialTimer)
{
}

void LinearEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
)
{
    _timer += dt;
    if (_timer >= stats.tickInterval)
    {
        _timer -= stats.tickInterval;

        float px = owner->GetX();
        float py = owner->GetY();
        Vector2 direction = owner->GetFacingDirection();

        // Auto-Targeting
        if (stats.targetRule == "Nearest")
        {
            auto monsters = room->GetMonstersInRange(px, py, 30.0f);
            if (!monsters.empty())
            {
                std::shared_ptr<Monster> nearest = nullptr;
                float minDistSq = std::numeric_limits<float>::max();

                for (auto &monster : monsters)
                {
                    if (monster->IsDead())
                        continue;
                    float dSq = Vector2::DistanceSq(Vector2(px, py), Vector2(monster->GetX(), monster->GetY()));
                    if (dSq < minDistSq)
                    {
                        minDistSq = dSq;
                        nearest = monster;
                    }
                }

                if (nearest)
                {
                    direction = Vector2(nearest->GetX() - px, nearest->GetY() - py);
                    if (!direction.IsZero())
                        direction.Normalize();
                    else
                        direction = owner->GetFacingDirection();
                }
            }
        }

        float speed = 15.0f * stats.speedMult;
        float life = 3.0f; // Could be configured via WeaponStats
        int32_t projectileCount = stats.projectileCount;

        for (int i = 0; i < projectileCount; ++i)
        {
            Vector2 fireDir = direction;
            if (projectileCount > 1)
            {
                float angleOffset = (static_cast<float>(i) / (projectileCount - 1) - 0.5f) * 0.5f;
                float s = std::sin(angleOffset);
                float c = std::cos(angleOffset);
                fireDir = Vector2(direction.x * c - direction.y * s, direction.x * s + direction.y * c);
            }

            float spawnOffset = owner->GetRadius() + 0.3f;
            float spawnX = px + fireDir.x * spawnOffset;
            float spawnY = py + fireDir.y * spawnOffset;

            auto proj = ProjectileFactory::Instance().CreateProjectile(
                room->GetObjectManager(),
                owner->GetId(),
                emitter->GetSkillId(),
                emitter->GetTypeId(),
                spawnX,
                spawnY,
                fireDir.x * speed,
                fireDir.y * speed,
                stats.damage,
                life
            );

            if (proj)
            {
                proj->SetRadius(0.2f);
                proj->SetPierce(stats.pierceCount);
                room->GetObjectManager().AddObject(proj);
                room->GetSpatialGrid().Add(proj);
                room->BroadcastSpawn({proj});
            }
        }
    }
}

} // namespace SimpleGame
