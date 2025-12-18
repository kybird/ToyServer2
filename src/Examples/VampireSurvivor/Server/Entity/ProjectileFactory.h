#pragma once
#include <memory>
#include "System/IObjectPool.h"

namespace SimpleGame {

class Projectile;
class ObjectManager;

/**
 * @brief Factory for creating Projectiles with pooling.
 */
class ProjectileFactory {
public:
    static ProjectileFactory& Instance();

    std::shared_ptr<Projectile> CreateProjectile(ObjectManager& objMgr, int32_t ownerId, int32_t skillId, float x, float y, float vx, float vy);
    void Release(Projectile* proj);

private:
    ProjectileFactory();
    ~ProjectileFactory() = default;

    std::unique_ptr<System::IObjectPool<Projectile>> _pool;
};

} // namespace SimpleGame
