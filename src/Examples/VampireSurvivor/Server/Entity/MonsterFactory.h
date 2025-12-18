#pragma once
#include <memory>
#include "System/IObjectPool.h"
#include "Entity/MonsterAIType.h"

namespace SimpleGame {

class Monster;
class ObjectManager;
class IAIBehavior;

/**
 * @brief Factory for creating monsters with appropriate AI.
 * 
 * Uses object pooling via IObjectPool interface.
 */
class MonsterFactory {
public:
    static MonsterFactory& Instance();

    /**
     * @brief Create a monster with stats from template and appropriate AI.
     */
    std::shared_ptr<Monster> CreateMonster(ObjectManager& objMgr, int32_t monsterTypeId, float x, float y);

    /**
     * @brief Release a monster back to the pool.
     */
    void Release(Monster* monster);

private:
    MonsterFactory();
    ~MonsterFactory() = default;
    
    // Prevent accidental copying
    MonsterFactory(const MonsterFactory&) = delete;
    MonsterFactory& operator=(const MonsterFactory&) = delete;

    /**
     * @brief Create AI behavior based on type.
     */
    std::unique_ptr<IAIBehavior> CreateAI(MonsterAIType type, float speed);

    std::unique_ptr<System::IObjectPool<Monster>> _pool;
};

} // namespace SimpleGame
