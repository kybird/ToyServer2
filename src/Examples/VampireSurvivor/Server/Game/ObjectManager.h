#pragma once
#include "Entity/GameObject.h"
#include "Entity/Player.h"
#include "Entity/Monster.h"
#include "Entity/Projectile.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>

namespace SimpleGame {

class ObjectManager
{
public:
    ObjectManager() = default;

    // ID Generaton
    int32_t GenerateId() {
        return ++_nextId; 
    }

    void AddObject(std::shared_ptr<GameObject> obj) {
        std::lock_guard<std::mutex> lock(_mutex);
        _objects[obj->GetId()] = obj;
    }

    void RemoveObject(int32_t id) {
        std::lock_guard<std::mutex> lock(_mutex);
        _objects.erase(id);
    }

    std::shared_ptr<GameObject> GetObject(int32_t id) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _objects.find(id);
        if (it != _objects.end()) return it->second;
        return nullptr;
    }

    // Snapshot for iteration (Thread-safe copy or use with caution)
    std::vector<std::shared_ptr<GameObject>> GetAllObjects() {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::shared_ptr<GameObject>> vec;
        vec.reserve(_objects.size());
        for(auto& pair : _objects) {
            vec.push_back(pair.second);
        }
        return vec;
    }

private:
    std::atomic<int32_t> _nextId{1000}; // Reserve 0-999 for Players or special
    std::unordered_map<int32_t, std::shared_ptr<GameObject>> _objects;
    std::mutex _mutex;
};

} // namespace SimpleGame
