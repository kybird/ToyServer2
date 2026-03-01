#pragma once
#include "Entity/Projectile.h"
#include "System/Memory/LockFreeObjectPool.h"
#include "System/Memory/RefPtr.h"

namespace SimpleGame {

class ObjectManager;

/**
 * @brief Factory for creating Projectiles with pooling.
 */
class ProjectileFactory
{
public:
    static ProjectileFactory &Instance();

    ::System::RefPtr<Projectile> CreateProjectile(
        ObjectManager &objMgr, int32_t ownerId, int32_t skillId, int32_t typeId, float x, float y, float vx, float vy,
        int32_t damage, float lifetime
    );
    void Release(Projectile *proj);

private:
    ProjectileFactory();
    ~ProjectileFactory() = default;

    ::System::LockFreeObjectPool<Projectile> _pool;
};

} // namespace SimpleGame
