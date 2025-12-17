#pragma once
#include "Entity/GameObject.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <mutex>

namespace SimpleGame {

// Sparse Grid
class SpatialGrid {
public:
    SpatialGrid(float cellSize) : _cellSize(cellSize) {}

    void Add(std::shared_ptr<GameObject> obj) {
        if(!obj) return;
        long long key = GetCellKey(obj->GetX(), obj->GetY());
        std::lock_guard<std::mutex> lock(_mutex);
        _cells[key].insert(obj);
    }

    void Remove(std::shared_ptr<GameObject> obj) {
         if(!obj) return;
        long long key = GetCellKey(obj->GetX(), obj->GetY());
        std::lock_guard<std::mutex> lock(_mutex);
        _cells[key].erase(obj);
    }

    void Update(std::shared_ptr<GameObject> obj, float oldX, float oldY) {
         if(!obj) return;
        long long oldKey = GetCellKey(oldX, oldY);
        long long newKey = GetCellKey(obj->GetX(), obj->GetY());

        if (oldKey != newKey) {
            std::lock_guard<std::mutex> lock(_mutex);
            _cells[oldKey].erase(obj);
            if (_cells[oldKey].empty()) {
                _cells.erase(oldKey); // Cleanup empty cells
            }
            _cells[newKey].insert(obj);
        }
    }

    std::vector<std::shared_ptr<GameObject>> QueryRange(float x, float y, float radius) {
        std::vector<std::shared_ptr<GameObject>> result;
        int minX = (int)std::floor((x - radius) / _cellSize);
        int maxX = (int)std::floor((x + radius) / _cellSize);
        int minY = (int)std::floor((y - radius) / _cellSize);
        int maxY = (int)std::floor((y + radius) / _cellSize);

        std::lock_guard<std::mutex> lock(_mutex);
        for (int cx = minX; cx <= maxX; cx++) {
            for (int cy = minY; cy <= maxY; cy++) {
                long long key = ((long long)cx << 32) | (unsigned int)cy;
                auto it = _cells.find(key);
                if (it != _cells.end()) {
                    for (auto& obj : it->second) {
                        float dx = obj->GetX() - x;
                        float dy = obj->GetY() - y;
                        if (dx*dx + dy*dy <= radius*radius) {
                            result.push_back(obj);
                        }
                    }
                }
            }
        }
        return result;
    }

private:
    float _cellSize;
    
    // Key: (x << 32) | y
    // Value: Set of objects in that cell
    std::unordered_map<long long, std::unordered_set<std::shared_ptr<GameObject>>> _cells;
    std::mutex _mutex;

    long long GetCellKey(float x, float y) {
        int cx = (int)std::floor(x / _cellSize);
        int cy = (int)std::floor(y / _cellSize);
        return ((long long)cx << 32) | (unsigned int)cy;
    }
};

} // namespace SimpleGame
