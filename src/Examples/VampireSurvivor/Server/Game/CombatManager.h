#pragma once
#include <memory>
#include <vector>

namespace SimpleGame {

class Room;
class Monster;
class Player;
class Projectile;
class ObjectManager;
class SpatialGrid;

class CombatManager
{
public:
    CombatManager();
    ~CombatManager();

    void Update(float dt, Room *room);

private:
    void ResolveProjectileCollisions(float dt, Room *room);
    void ResolveBodyCollisions(float dt, Room *room);
    void ResolveCleanup(Room *room);
    void ApplyKnockback(std::shared_ptr<Monster> monster, std::shared_ptr<Player> player);

private:
    const float KNOCKBACK_FORCE = 15.0f;
};

} // namespace SimpleGame
