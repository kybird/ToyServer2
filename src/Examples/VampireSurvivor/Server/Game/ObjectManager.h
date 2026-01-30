#pragma once
#include "Entity/GameObject.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace SimpleGame {

class ObjectManager
{
public:
    ObjectManager() = default;

    // ID Generaton
    int32_t GenerateId()
    {
        return ++_nextId;
    }

    void AddObject(std::shared_ptr<GameObject> obj)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _objects[obj->GetId()] = obj;

        // [NEW] 몬스터면 카운트 증가
        if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {
            ++_aliveMonsterCount;
        }
    }

    void RemoveObject(int32_t id)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _objects.find(id);
        if (it != _objects.end())
        {
            // [NEW] 몬스터면 카운트 감소
            if (it->second->GetType() == Protocol::ObjectType::MONSTER)
            {
#ifdef _DEBUG
                assert(_aliveMonsterCount > 0 && "MonsterCount underflow!");
#endif
                if (_aliveMonsterCount > 0)
                    --_aliveMonsterCount;
            }
            _objects.erase(it);
        }
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _objects.clear();
        _aliveMonsterCount = 0;
    }

    std::shared_ptr<GameObject> GetObject(int32_t id)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _objects.find(id);
        if (it != _objects.end())
            return it->second;
        return nullptr;
    }

    size_t GetObjectCount()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _objects.size();
    }

    // Snapshot for iteration (Thread-safe copy or use with caution)
    std::vector<std::shared_ptr<GameObject>> GetAllObjects()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::shared_ptr<GameObject>> vec;
        vec.reserve(_objects.size());
        for (auto &pair : _objects)
        {
            vec.push_back(pair.second);
        }
        return vec;
    }

    int32_t GetAliveMonsterCount() const
    {
        return _aliveMonsterCount;
    }

private:
    std::atomic<int32_t> _nextId{1000}; // Reserve 0-999 for Players or special
    std::unordered_map<int32_t, std::shared_ptr<GameObject>> _objects;
    std::mutex _mutex;
    int32_t _aliveMonsterCount = 0;
};

} // namespace SimpleGame
