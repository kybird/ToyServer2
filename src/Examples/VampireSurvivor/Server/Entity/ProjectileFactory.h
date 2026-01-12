#pragma once
#include "System/IObjectPool.h"
#include <memory>

namespace SimpleGame {

class Projectile;
class ObjectManager;

/**
 * @brief Factory for creating Projectiles with pooling.
 */
class ProjectileFactory
{
public:
    static ProjectileFactory &Instance();

    std::shared_ptr<Projectile> CreateProjectile(
        ObjectManager &objMgr, int32_t ownerId, int32_t skillId, int32_t typeId, float x, float y, float vx, float vy,
        int32_t damage, float lifetime
    );
    void Release(Projectile *proj);

private:
    ProjectileFactory();
    ~ProjectileFactory() = default;

    std::unique_ptr<System::IObjectPool<Projectile>> _pool;
};

} // namespace SimpleGame
