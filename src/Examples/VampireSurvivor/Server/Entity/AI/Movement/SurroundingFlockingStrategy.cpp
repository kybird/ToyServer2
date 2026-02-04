#include "SurroundingFlockingStrategy.h"
#include "Entity/Monster.h"
#include "Game/Room.h"
#include <algorithm>
#include <cmath>

namespace SimpleGame {

void SurroundingFlockingStrategy::CalculateMovement(
    Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
)
{
    float mx = monster->GetX();
    float my = monster->GetY();
    float dx = targetX - mx;
    float dy = targetY - my;
    float distSq = dx * dx + dy * dy;

    // 플레이어와 너무 가까우면 정지
    if (distSq < 0.25f)
    {
        outVx = 0;
        outVy = 0;
        return;
    }

    float dist = std::sqrt(distSq);
    float nx = dx / dist; // Chase direction (to player)
    float ny = dy / dist;

    // 1. 주변 몬스터 탐색 및 분리 로직
    // 반지름의 약 2.2배 정도를 체크 범위로 설정
    float checkRadius = monster->GetRadius() * 2.2f;
    auto neighbors = room->GetMonstersInRange(mx, my, checkRadius);

    float sepX = 0, sepY = 0;
    int sepCount = 0;
    bool blockedAhead = false;

    // 내 ID에 따라 선호하는 회전 방향 결정 (0: 시계방향, 1: 반시계방향)
    // 이를 통해 유저를 양옆으로 나눠서 감싸는 효과 유도
    bool preferCCW = (monster->GetId() % 2 == 0);

    for (auto &neighbor : neighbors)
    {
        if (neighbor.get() == monster)
            continue;

        float ox = neighbor->GetX() - mx;
        float oy = neighbor->GetY() - my;
        float d2 = ox * ox + oy * oy;
        float rSum = monster->GetRadius() + neighbor->GetRadius();

        // 약간의 여유(1.1배)를 둔 충돌 범위
        float minSep = rSum * 1.1f;

        if (d2 < minSep * minSep && d2 > 0.0001f)
        {
            float d = std::sqrt(d2);
            float overlap = minSep - d;

            // Separation: 중심에서 멀어지는 방향
            sepX -= (ox / d) * overlap;
            sepY -= (oy / d) * overlap;
            sepCount++;

            // 정면(약 45도 이내)에 다른 몬스터가 있는지 확인
            float dot = (ox / d) * nx + (oy / d) * ny;
            if (dot > 0.707f) // cos(45도)
            {
                blockedAhead = true;
            }
        }
    }

    float speed = monster->GetSpeed();

    // 2. 이동 벡터 결정
    if (blockedAhead)
    {
        // 전방이 막혔을 때: 멈추지 않고 선호하는 방향으로 회전하여 우회 ("둘러싸기")
        float tx, ty;
        if (preferCCW)
        {
            // 반시계방향 법선 ( -y, x )
            tx = -ny;
            ty = nx;
        }
        else
        {
            // 시계방향 법선 ( y, -x )
            tx = ny;
            ty = -nx;
        }

        // 전방 벡터와 회전 벡터를 적절히 섞어 빗겨나가는 움직임 구현
        // 섞는 비율을 조절하여 둘러싸는 곡선의 반경 제어
        nx = nx * 0.4f + tx * 0.8f;
        ny = ny * 0.4f + ty * 0.8f;

        // 우회 시에는 밀도를 유지하기 위해 속도를 소폭 감소
        speed *= 0.95f;
    }
    else if (sepCount > 0)
    {
        // 일반적인 분리 가중치 적용
        nx = nx * 0.7f + (sepX / sepCount) * 0.3f;
        ny = ny * 0.7f + (sepY / sepCount) * 0.3f;
    }

    // 최종 벡터 정규화
    float finalLen = std::sqrt(nx * nx + ny * ny);
    if (finalLen > 0.001f)
    {
        nx /= finalLen;
        ny /= finalLen;
    }

    // 3. 관성(Inertia) 적용: 급격한 방향 전환으로 인한 떨림 방지
    float finalTargetVx = nx * speed;
    float finalTargetVy = ny * speed;

    // 이전 프레임의 속도를 85% 유지 (부드러운 곡선 이동 유도)
    outVx = monster->GetVX() * 0.85f + finalTargetVx * 0.15f;
    outVy = monster->GetVY() * 0.85f + finalTargetVy * 0.15f;
}

} // namespace SimpleGame
