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

} // namespace SimpleGame
