#pragma once
#include <vector>
#include <memory>
#include <string>

namespace SimpleGame {

class Monster;      // Forward Declaration
class Room;         // Forward Declaration

enum class NodeStatus {
    SUCCESS,
    FAILURE,
    RUNNING
};

class BehaviorNode {
public:
    virtual ~BehaviorNode() = default;
    virtual NodeStatus Tick(Monster* monster, Room* room, float dt) = 0;
};

// Composite Node
class CompositeNode : public BehaviorNode {
public:
    void AddChild(std::shared_ptr<BehaviorNode> node) {
        _children.push_back(node);
    }
protected:
    std::vector<std::shared_ptr<BehaviorNode>> _children;
};

// Selector: Runs children until one succeeds
class Selector : public CompositeNode {
public:
    NodeStatus Tick(Monster* monster, Room* room, float dt) override {
        for (auto& child : _children) {
            NodeStatus status = child->Tick(monster, room, dt);
            if (status != NodeStatus::FAILURE) {
                return status; // Success or Running
            }
        }
        return NodeStatus::FAILURE;
    }
};

// Sequence: Runs children until one fails
class Sequence : public CompositeNode {
public:
    NodeStatus Tick(Monster* monster, Room* room, float dt) override {
        for (auto& child : _children) {
            NodeStatus status = child->Tick(monster, room, dt);
            if (status != NodeStatus::SUCCESS) {
                return status; // Failure or Running
            }
        }
        return NodeStatus::SUCCESS;
    }
};

} // namespace SimpleGame
