#include "Entity/ProjectileFactory.h"
#include "Entity/Projectile.h"
#include "Game/ObjectManager.h"
#include "System/Memory/SimplePool.h"

namespace SimpleGame {

ProjectileFactory &ProjectileFactory::Instance()
{
    static ProjectileFactory instance;
    return instance;
}

ProjectileFactory::ProjectileFactory()
{
    _pool = std::make_unique<System::SimplePool<Projectile>>(2000);
}

std::shared_ptr<Projectile> ProjectileFactory::CreateProjectile(
    ObjectManager &objMgr, int32_t ownerId, int32_t skillId, int32_t typeId, float x, float y, float vx, float vy,
    int32_t damage, float lifetime
)
{
    Projectile *raw = _pool->Acquire();
    if (!raw)
        return nullptr;

    std::shared_ptr<Projectile> proj(
        raw,
        [](Projectile *p)
        {
            ProjectileFactory::Instance().Release(p);
        }
    );

    int32_t id = objMgr.GenerateId();
    proj->Initialize(id, ownerId, skillId, typeId);
    proj->SetPos(x, y);
    proj->SetVelocity(vx, vy);
    proj->SetDamage(damage);
    proj->SetLifetime(lifetime);

    return proj;
}

void ProjectileFactory::Release(Projectile *proj)
{
    if (proj)
    {
        proj->Reset();
        _pool->Release(proj);
    }
}

} // namespace SimpleGame
