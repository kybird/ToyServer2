#pragma once
#include "../System/ObjectManager.h"
#include "Projectile.h"
#include "System/IObjectPool.h"
#include "System/Memory/SimplePool.h"

namespace SimpleGame {

/**
 * @brief Factory for creating Projectiles with pooling.
 */
class ProjectileFactory {
public:
    static ProjectileFactory& Instance() {
        static ProjectileFactory instance;
        return instance;
    }

    std::shared_ptr<Projectile> CreateProjectile(ObjectManager& objMgr, int32_t ownerId, int32_t skillId, float x, float y, float vx, float vy) {
        Projectile* raw = _pool.Acquire();
        if (!raw) return nullptr;

        std::shared_ptr<Projectile> proj(raw, [](Projectile* p) {
            ProjectileFactory::Instance().Release(p);
        });

        int32_t id = objMgr.GenerateId();
        proj->Initialize(id, ownerId, skillId);
        proj->SetPos(x, y);
        proj->SetVelocity(vx, vy);

        return proj;
    }

    void Release(Projectile* proj) {
        proj->Reset();
        _pool.Release(proj);
    }

private:
    ProjectileFactory() : _pool(2000) {} // High limit for projectiles

    System::SimplePool<Projectile> _pool;
};

} // namespace SimpleGame
