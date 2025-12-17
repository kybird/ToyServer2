#include "Entity/AI/IAIBehavior.h"
#include "Entity/AI/BehaviorTree/BehaviorTree.h"
#include "Entity/MonsterAIType.h"
#include <memory>

namespace SimpleGame {

class Monster;
class Room;

/**
 * @brief Boss AI using Behavior Tree.
 * 
 * Executes a behavior tree logic every Think/Execute cycle.
 */
class BossAI : public IAIBehavior {
public:
    BossAI();

    void Think(Monster* monster, Room* room, float currentTime) override;
    void Execute(Monster* monster, float dt) override;
    void Reset() override;

private:
    std::shared_ptr<BehaviorNode> _root;
    void BuildTree();
};

} // namespace SimpleGame
