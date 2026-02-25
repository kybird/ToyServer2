#include "SpatialGrid.h"
#include "ObjectManager.h"

namespace SimpleGame {

void SpatialGrid::QueryRange(
    float x, float y, float radius, std::vector<std::shared_ptr<GameObject>> &outResults, ObjectManager &objMgr
)
{
    outResults.clear();
    float radiusSq = radius * radius;

    int minX = static_cast<int>(std::floor((x - radius) / _cellSize));
    int maxX = static_cast<int>(std::floor((x + radius) / _cellSize));
    int minY = static_cast<int>(std::floor((y - radius) / _cellSize));
    int maxY = static_cast<int>(std::floor((y + radius) / _cellSize));

    for (int cx = minX; cx <= maxX; cx++)
    {
        for (int cy = minY; cy <= maxY; cy++)
        {
            unsigned int ux = static_cast<unsigned int>(cx + OFFSET) % GRID_SIZE;
            unsigned int uy = static_cast<unsigned int>(cy + OFFSET) % GRID_SIZE;
            int idx = static_cast<int>(ux * GRID_SIZE + uy);

            const auto &cell = _cells[static_cast<size_t>(idx)];
            for (int32_t id : cell.monsterIds)
            {
                auto obj = objMgr.GetObject(id);
                if (obj)
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

void SpatialGrid::Rebuild(const std::vector<std::shared_ptr<GameObject>> &objects)
{
    // 1. 기존 데이터 초기화 (메모리 재할당 방지를 위해 clear만 수행)
    for (auto &cell : _cells)
    {
        cell.monsterIds.clear();
    }

    // 2. 살아있는 몬스터들을 격자에 추가
    for (const auto &obj : objects)
    {
        if (!obj || obj->GetType() != Protocol::ObjectType::MONSTER || obj->GetState() == Protocol::ObjectState::DEAD)
            continue;

        int idx = GetIndex(obj->GetX(), obj->GetY());
        _cells[static_cast<size_t>(idx)].monsterIds.push_back(obj->GetId());
    }
}

void SpatialGrid::GetNeighborCells(int cellIdx, std::array<int, 9> &outNeighbors) const
{
    int cx = cellIdx / GRID_SIZE;
    int cy = cellIdx % GRID_SIZE;

    int count = 0;
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            unsigned int ux = static_cast<unsigned int>(cx + dx) % GRID_SIZE;
            unsigned int uy = static_cast<unsigned int>(cy + dy) % GRID_SIZE;
            outNeighbors[count++] = static_cast<int>(ux * GRID_SIZE + uy);
        }
    }
}

void SpatialGrid::Clear()
{
    for (auto &cell : _cells)
        cell.monsterIds.clear();
}

void SpatialGrid::HardClear()
{
    for (auto &cell : _cells)
    {
        cell.monsterIds.clear();
        cell.monsterIds.shrink_to_fit();
    }
}

} // namespace SimpleGame
