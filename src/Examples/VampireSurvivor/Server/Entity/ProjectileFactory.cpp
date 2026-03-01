#include "Entity/ProjectileFactory.h"

#include "Game/ObjectManager.h"

namespace SimpleGame {

ProjectileFactory &ProjectileFactory::Instance()
{
    static ProjectileFactory instance;
    return instance;
}

ProjectileFactory::ProjectileFactory()
{
    _pool.Init(0, 20000);
}

::System::RefPtr<Projectile> ProjectileFactory::CreateProjectile(
    ObjectManager &objMgr, int32_t ownerId, int32_t skillId, int32_t typeId, float x, float y, float vx, float vy,
    int32_t damage, float lifetime
)
{
    ::System::RefPtr<Projectile> proj = _pool.Pop();
    if (!proj)
        return nullptr;

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
        _pool.Push(proj);
    }
}

} // namespace SimpleGame
