#pragma once
#include "Entity/Monster.h"
#include "Entity/MonsterAIType.h"
#include "System/Memory/LockFreeObjectPool.h"
#include "System/Memory/RefPtr.h"
#include <vector>

namespace SimpleGame {

class ObjectManager;
class IAIBehavior;

/**
 * @brief Factory for creating monsters with appropriate AI.
 *
 * Uses object pooling via IObjectPool interface.
 */
class MonsterFactory
{
public:
    static MonsterFactory &Instance();

    /**
     * @brief Create a monster with stats from template and appropriate AI.
     */
    ::System::RefPtr<Monster>
    CreateMonster(ObjectManager &objMgr, int32_t monsterTypeId, float x, float y, int32_t hpOverride = 0);

    /**
     * @brief Spawn multiple monsters efficiently.
     * @return List of spawned monsters.
     */
    std::vector<::System::RefPtr<Monster>>
    SpawnBatch(ObjectManager &objMgr, int32_t monsterTypeId, int count, float minX, float maxX, float minY, float maxY);

    /**
     * @brief Release a monster back to the pool.
     */
    void Release(Monster *monster);

private:
    MonsterFactory();
    ~MonsterFactory() = default;

    // Prevent accidental copying
    MonsterFactory(const MonsterFactory &) = delete;
    MonsterFactory &operator=(const MonsterFactory &) = delete;

    /**
     * @brief Create AI behavior based on type.
     */
    std::unique_ptr<IAIBehavior> CreateAI(MonsterAIType type, float speed);

    std::unique_ptr<::System::LockFreeObjectPool<Monster>> _pool;
};

} // namespace SimpleGame
