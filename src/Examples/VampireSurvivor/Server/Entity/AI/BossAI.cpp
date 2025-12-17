#include "Entity/AI/BossAI.h"
#include "Entity/Monster.h"
#include "Game/Room.h"
#include <cmath>

namespace SimpleGame {

BossAI::BossAI() {
    BuildTree();
}

void BossAI::Think(Monster* monster, Room* room, float currentTime) {
    if (_root) {
        _root->Tick(monster, room, 0.0f);
    }
}

void BossAI::Execute(Monster* monster, float dt) {
    if (_root) {
        _root->Tick(monster, nullptr, dt);
    }
}

void BossAI::Reset() {
    // Reset tree logic if needed
}

// Internal Node Classes for Boss Logic
namespace {

    class ChaseNode : public BehaviorNode {
    public:
        NodeStatus Tick(Monster* monster, Room* room, float dt) override {
            // If dt is provided (Execute phase), we might just move.
            // But Chase logic usually needs target info which depends on Room.
            // In Execute(monster, nullptr, dt), room is null.
            
            // Hybrid approach adaptation for BT:
            // This is a simplified demo node. 
            // Real BT nodes might store state in Blackboard.
            
            // If called from Think (room exists, dt=0) -> Find target & Set Velocity
            if (room) {
                auto target = room->GetNearestPlayer(monster->GetX(), monster->GetY());
                if (!target) return NodeStatus::FAILURE;

                float dx = target->GetX() - monster->GetX();
                float dy = target->GetY() - monster->GetY();
                float distSq = dx*dx + dy*dy;
                
                if (distSq > 0.1f) {
                    float dist = std::sqrt(distSq);
                    monster->SetVelocity((dx/dist)*1.5f, (dy/dist)*1.5f);
                    return NodeStatus::RUNNING;
                }
                return NodeStatus::SUCCESS;
            }
            
            // If called from Execute (room null, dt>0) -> Just keep moving (Velocity already set)
            // Or we could rely on Monster's velocity integration which happens in Physics update.
            return NodeStatus::RUNNING;
        }
    };

}

void BossAI::BuildTree() {
    auto root = std::make_shared<Selector>();
    
    // Add behaviors
    root->AddChild(std::make_shared<ChaseNode>());
    
    _root = root;
}

} // namespace SimpleGame
