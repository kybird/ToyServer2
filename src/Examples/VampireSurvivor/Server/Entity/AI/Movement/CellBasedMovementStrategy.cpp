#include "CellBasedMovementStrategy.h"
#include "Entity/Monster.h"
#include "Game/Room.h"
#include <cmath>
#include <vector>

namespace SimpleGame::Movement {

void CellBasedMovementStrategy::CalculateMovement(
    Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
)
{
    float mx = monster->GetX();
    float my = monster->GetY();
    float dx = targetX - mx;
    float dy = targetY - my;
    float distSq = dx * dx + dy * dy;

    if (distSq < 0.25f)
    {
        outVx = 0;
        outVy = 0;
        return;
    }

    float speed = monster->GetSpeed();
    float dist = std::sqrt(distSq);
    float nx = dx / dist;
    float ny = dy / dist;

    // 타겟 셀 (플레이어 방향 1칸 앞)
    int targetCellX = static_cast<int>(std::floor(mx + nx * 1.0f));
    int targetCellY = static_cast<int>(std::floor(my + ny * 1.0f));

    int bestX = targetCellX;
    int bestY = targetCellY;
    bool found = false;

    if (room != nullptr)
    {
        int currentCellX = static_cast<int>(std::floor(mx));
        int currentCellY = static_cast<int>(std::floor(my));

        // 우선순위: 전방 1칸 -> 대각선 전방 2칸 -> 좌우 2칸 -> 대각 후방 2칸 -> 후방 1칸 -> 제자리
        // 총 9칸 탐색 (O(1))
        std::pair<int, int> candidates[9] = {
            {targetCellX, targetCellY},
            {targetCellX + 1, targetCellY},
            {targetCellX - 1, targetCellY},
            {targetCellX, targetCellY + 1},
            {targetCellX, targetCellY - 1},
            {targetCellX + 1, targetCellY + 1},
            {targetCellX - 1, targetCellY + 1},
            {targetCellX + 1, targetCellY - 1},
            {targetCellX - 1, targetCellY - 1}
        };

        float minDistToTarget = 999999.0f;

        for (int i = 0; i < 9; ++i)
        {
            int cx = candidates[i].first;
            int cy = candidates[i].second;

            if (!room->IsCellOccupied(cx, cy))
            {
                float cellCenterX = cx + 0.5f;
                float cellCenterY = cy + 0.5f;
                float tx = targetX - cellCenterX;
                float ty = targetY - cellCenterY;
                float tdSq = tx * tx + ty * ty;

                if (tdSq < minDistToTarget)
                {
                    minDistToTarget = tdSq;
                    bestX = cx;
                    bestY = cy;
                    found = true;
                }
            }
        }

        if (!found)
        {
            bestX = currentCellX;
            bestY = currentCellY;
        }

        // O(1) Occupancy Map 등록
        room->OccupyCell(bestX, bestY);
    }

    // 예약한 슬롯의 중앙으로 보정 및 이동
    float moveTargetX = bestX + 0.5f;
    float moveTargetY = bestY + 0.5f;

    // 타겟을 향한 실제 물리 벡터 계산
    float vdx = moveTargetX - mx;
    float vdy = moveTargetY - my;
    float vDist = std::sqrt(vdx * vdx + vdy * vdy);

    float targetVx = 0.0f;
    float targetVy = 0.0f;

    if (vDist > 0.05f) // 거리에 완전히 도달하지 않은 경우 속도 부여
    {
        float currentSpeed = speed;
        // 목표점에 가까워질수록 감속 (Arrival Behavior)
        if (vDist < 0.5f)
        {
            currentSpeed = speed * (vDist / 0.5f);
        }
        targetVx = (vdx / vDist) * currentSpeed;
        targetVy = (vdy / vDist) * currentSpeed;

        // 관성을 통한 부드러운 움직임 (Jitter 최소화)
        outVx = monster->GetVX() * 0.8f + targetVx * 0.2f;
        outVy = monster->GetVY() * 0.8f + targetVy * 0.2f;
    }
    else
    {
        // 거의 도착했으면 완전 정지시켜서 불필요한 좌우 반전(Jitter) 원천 차단
        outVx = 0.0f;
        outVy = 0.0f;
    }
}

} // namespace SimpleGame::Movement
