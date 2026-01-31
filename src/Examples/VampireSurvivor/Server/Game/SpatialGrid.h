#pragma once
#include "Entity/GameObject.h"
#include <cmath>
#include <unordered_map>
#include <vector>

namespace SimpleGame {

// Sparse Grid
class SpatialGrid
{
public:
    SpatialGrid(float cellSize) : _cellSize(cellSize)
    {
    }

    // [Optimization] Removed Internal Mutex (Room already locks this via Strand/RoomMutex)
    void Add(std::shared_ptr<GameObject> obj)
    {
        if (!obj || obj->IsInGrid())
            return;

        long long key = GetCellKey(obj->GetX(), obj->GetY());
        auto &vec = _cells[key];

        // Double check uniqueness (though obj->IsInGrid() should handle it)
        for (const auto &existing : vec)
        {
            if (existing == obj)
                return;
        }

        vec.push_back(obj);
        obj->SetGridInfo(key, true);
    }

    void Remove(std::shared_ptr<GameObject> obj)
    {
        if (!obj || !obj->IsInGrid())
            return;

        long long key = obj->GetGridCellKey();
        auto it = _cells.find(key);
        if (it != _cells.end())
        {
            auto &vec = it->second;
            for (size_t i = 0; i < vec.size(); ++i)
            {
                if (vec[i] == obj)
                {
                    if (i != vec.size() - 1)
                    {
                        std::swap(vec[i], vec.back());
                    }
                    vec.pop_back();
                    if (vec.empty())
                    {
                        _cells.erase(it);
                    }
                    break;
                }
            }
        }
        obj->SetGridInfo(0, false);
    }

    void Update(std::shared_ptr<GameObject> obj)
    {
        if (!obj)
            return;

        // If not in grid, just Add it
        if (!obj->IsInGrid())
        {
            Add(obj);
            return;
        }

        long long oldKey = obj->GetGridCellKey();
        long long newKey = GetCellKey(obj->GetX(), obj->GetY());

        if (oldKey != newKey)
        {
            // Remove from old cell (using stored key)
            auto oldIt = _cells.find(oldKey);
            if (oldIt != _cells.end())
            {
                auto &vec = oldIt->second;
                for (size_t i = 0; i < vec.size(); ++i)
                {
                    if (vec[i] == obj)
                    {
                        if (i != vec.size() - 1)
                        {
                            std::swap(vec[i], vec.back());
                        }
                        vec.pop_back();
                        if (vec.empty())
                        {
                            _cells.erase(oldIt);
                        }
                        break;
                    }
                }
            }

            // Add to new cell
            auto &newVec = _cells[newKey];
            newVec.push_back(obj);
            obj->SetGridInfo(newKey, true);
        }
    }

    void QueryRange(float x, float y, float radius, std::vector<std::shared_ptr<GameObject>> &outResults)
    {
        outResults.clear();
        // Reserve some space to avoid reallocations
        if (outResults.capacity() < 64)
            outResults.reserve(64);

        int minX = (int)std::floor((x - radius) / _cellSize);
        int maxX = (int)std::floor((x + radius) / _cellSize);
        int minY = (int)std::floor((y - radius) / _cellSize);
        int maxY = (int)std::floor((y + radius) / _cellSize);

        float radiusSq = radius * radius;

        for (int cx = minX; cx <= maxX; cx++)
        {
            for (int cy = minY; cy <= maxY; cy++)
            {
                long long key = ((long long)cx << 32) | (unsigned int)cy;
                auto it = _cells.find(key);
                if (it != _cells.end())
                {
                    const auto &vec = it->second;
                    for (const auto &obj : vec)
                    {
                        float dx = obj->GetX() - x;
                        float dy = obj->GetY() - y;
                        if (dx * dx + dy * dy <= radiusSq)
                        {
                            outResults.push_back(obj);
                        }
                    }
                }
            }
        }
    }

    void Clear()
    {
        // When clearing grid, reset all objects' grid info
        for (auto &pair : _cells)
        {
            for (auto &obj : pair.second)
            {
                obj->SetGridInfo(0, false);
            }
        }
        _cells.clear();
    }

private:
    float _cellSize;

    // Key: (x << 32) | y
    // Value: Vector of objects (Cache friendly)
    // Using unordered_map for spatial hashing with vector for fast local iteration.
    std::unordered_map<long long, std::vector<std::shared_ptr<GameObject>>> _cells;

    long long GetCellKey(float x, float y) const
    {
        int cx = (int)std::floor(x / _cellSize);
        int cy = (int)std::floor(y / _cellSize);
        return ((long long)cx << 32) | (unsigned int)cy;
    }
};

} // namespace SimpleGame
