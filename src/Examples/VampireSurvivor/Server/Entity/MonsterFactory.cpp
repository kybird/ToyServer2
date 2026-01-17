#include "Entity/MonsterFactory.h"
#include "Core/DataManager.h"
#include "Entity/AI/BossAI.h"
#include "Entity/AI/ChaserAI.h"
#include "Entity/AI/SwarmAI.h"
#include "Entity/AI/WanderAI.h"
#include "Entity/Monster.h"
#include "Game/ObjectManager.h"
#include "System/ILog.h"
#include "System/Memory/SimplePool.h"

namespace SimpleGame {

MonsterFactory &MonsterFactory::Instance()
{
    static MonsterFactory instance;
    return instance;
}

MonsterFactory::MonsterFactory()
{
    _pool = std::make_unique<System::SimplePool<Monster>>(1000);
}

std::shared_ptr<Monster>
MonsterFactory::CreateMonster(ObjectManager &objMgr, int32_t monsterTypeId, float x, float y, int32_t hpOverride)
{
    const auto *tmpl = DataManager::Instance().GetMonsterTemplate(monsterTypeId);
    if (!tmpl)
    {
        LOG_ERROR("Invalid Monster Type ID: {}", monsterTypeId);
        return nullptr;
    }

    int32_t finalHp = (hpOverride > 0) ? hpOverride : tmpl->hp;

    // Acquire from pool or alloc
    Monster *raw = _pool->Acquire();
    if (!raw)
    {
        // Should not happen with current SimplePool unless alloc limit reached
        return nullptr;
    }

    int32_t id = objMgr.GenerateId();

    // Custom deleter returns to pool
    std::shared_ptr<Monster> monster(
        raw,
        [](Monster *m)
        {
            MonsterFactory::Instance().Release(m);
        }
    );

    // Init
    monster->Initialize(
        id, monsterTypeId, finalHp, tmpl->radius, tmpl->damageOnContact, tmpl->attackCooldown, tmpl->speed
    );

    // Apply Velocity/Pos
    monster->SetVelocity(0, 0);
    monster->SetPos(x, y);

    // Create AI based on template
    monster->SetAI(CreateAI(tmpl->aiType, tmpl->speed));

    return monster;
}

std::vector<std::shared_ptr<Monster>> MonsterFactory::SpawnBatch(
    ObjectManager &objMgr, int32_t monsterTypeId, int count, float minX, float maxX, float minY, float maxY
)
{
    std::vector<std::shared_ptr<Monster>> monsters;
    monsters.reserve(count);

    // Naive Random for now (Can be improved with proper C++ random if needed)
    for (int i = 0; i < count; ++i)
    {
        float r1 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float r2 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float x = minX + r1 * (maxX - minX);
        float y = minY + r2 * (maxY - minY);

        auto monster = CreateMonster(objMgr, monsterTypeId, x, y);
        if (monster)
        {
            monsters.push_back(monster);
        }
    }
    return monsters;
}

void MonsterFactory::Release(Monster *monster)
{
    if (monster)
    {
        monster->Reset();
        _pool->Release(monster);
    }
}

std::unique_ptr<IAIBehavior> MonsterFactory::CreateAI(MonsterAIType type, float speed)
{
    switch (type)
    {
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
