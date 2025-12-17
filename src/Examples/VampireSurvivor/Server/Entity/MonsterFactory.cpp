#include "Entity/MonsterFactory.h"
#include "Game/ObjectManager.h"
#include "Core/DataManager.h"
#include "Entity/Monster.h"
#include "Entity/AI/ChaserAI.h"
#include "Entity/AI/WanderAI.h"
#include "Entity/AI/SwarmAI.h"
#include "Entity/AI/BossAI.h"
#include "System/ILog.h"

namespace SimpleGame {

MonsterFactory& MonsterFactory::Instance() {
    static MonsterFactory instance;
    return instance;
}

MonsterFactory::MonsterFactory() : _pool(1000) {
    // Pool limit 1000
}

std::shared_ptr<Monster> MonsterFactory::CreateMonster(ObjectManager& objMgr, int32_t monsterTypeId, float x, float y) {
    const auto* tmpl = DataManager::Instance().GetMonsterTemplate(monsterTypeId);
    if (!tmpl) {
        LOG_ERROR("Invalid Monster Type ID: {}", monsterTypeId);
        return nullptr;
    }

    // Acquire from pool or alloc
    Monster* raw = _pool.Acquire();
    if (!raw) {
        // Should not happen with current SimplePool unless alloc limit reached
        return nullptr;
    }

    int32_t id = objMgr.GenerateId();
    
    // Custom deleter returns to pool
    std::shared_ptr<Monster> monster(raw, [](Monster* m) {
        MonsterFactory::Instance().Release(m);
    });

    // Init
    monster->Initialize(id, monsterTypeId); 

    // Apply Template Stats
    monster->SetHp(tmpl->hp);
    monster->SetVelocity(0, 0);
    monster->SetPos(x, y);

    // Create AI based on template
    monster->SetAI(CreateAI(tmpl->aiType, tmpl->speed));

    return monster;
}

void MonsterFactory::Release(Monster* monster) {
    if (monster) {
        monster->Reset();
        _pool.Release(monster);
    }
}

std::unique_ptr<IAIBehavior> MonsterFactory::CreateAI(MonsterAIType type, float speed) {
    switch (type) {
        case MonsterAIType::CHASER:
            return std::make_unique<ChaserAI>(speed);
        case MonsterAIType::WANDER:
            return std::make_unique<WanderAI>(speed);
        case MonsterAIType::SWARM:
            return std::make_unique<SwarmAI>(speed);
        case MonsterAIType::BOSS:
            return std::make_unique<BossAI>();
        default:
            return std::make_unique<ChaserAI>(speed);
    }
}

} // namespace SimpleGame
