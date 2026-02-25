#pragma once
#include "Entity/GameObject.h"
#include "Protocol/game.pb.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace SimpleGame {

// Forward declarations
class ObjectManager;

struct CellData
{
    std::vector<int32_t> monsterIds; // 몬스터 ID 저장 (ObjectManager API와 일치)
};

class SpatialGrid
{
public:
    // 격자 크기 및 오프셋 정의
    static constexpr int GRID_SIZE = 256;  // 256x256 격자
    static constexpr int OFFSET = 1000000; // 음수 좌표 처리를 위한 오프셋
    static constexpr int TOTAL_CELLS = GRID_SIZE * GRID_SIZE;

    SpatialGrid(float cellSize) : _cellSize(cellSize)
    {
        _cells.resize(TOTAL_CELLS);
    }

    // [Optimization] 모든 객체를 격자에 일괄 재배치 (O(N))
    void Rebuild(const std::vector<std::shared_ptr<GameObject>> &objects);

    // 좌표를 격자 인덱스로 변환 (Wrap-around 방식 지원)
    inline int GetIndex(float x, float y) const
    {
        int cx = static_cast<int>(std::floor(x / _cellSize));
        int cy = static_cast<int>(std::floor(y / _cellSize));

        unsigned int ux = static_cast<unsigned int>(cx + OFFSET) % GRID_SIZE;
        unsigned int uy = static_cast<unsigned int>(cy + OFFSET) % GRID_SIZE;
        return static_cast<int>(ux * GRID_SIZE + uy);
    }

    // 인덱스를 통해 인접한 9개 셀(자신 포함)의 인덱스를 가져옴
    void GetNeighborCells(int cellIdx, std::array<int, 9> &outNeighbors) const;

    const std::vector<int32_t> &GetMonsterIds(int cellIdx) const
    {
        return _cells[static_cast<size_t>(cellIdx)].monsterIds;
    }

    // QueryRange (CombatManager 등에서 사용)
    void QueryRange(
        float x, float y, float radius, std::vector<std::shared_ptr<GameObject>> &outResults, ObjectManager &objMgr
    );

    void Clear();

    // [New] 메모리까지 완전히 해제 (방 리셋 시 사용)
    void HardClear();

private:
    float _cellSize;
    std::vector<CellData> _cells;
};

} // namespace SimpleGame
