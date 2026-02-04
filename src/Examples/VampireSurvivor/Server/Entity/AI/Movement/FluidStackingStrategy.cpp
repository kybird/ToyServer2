#include "FluidStackingStrategy.h"
#include "Entity/Monster.h"
#include "Game/Room.h"
#include "System/ILog.h"
#include <algorithm>
#include <cmath>

namespace SimpleGame {

void FluidStackingStrategy::CalculateMovement(
    Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
)
{
    float mx = monster->GetX();
    float my = monster->GetY();
    float dx = targetX - mx;
    float dy = targetY - my;
    float distSq = dx * dx + dy * dy;

    if (distSq < 0.1f)
    {
        outVx = 0;
        outVy = 0;
        return;
    }

    float dist = std::sqrt(distSq);
    float nx = dx / dist;
    float ny = dy / dist;

    // 주변 몬스터 탐색 (범위는 충돌 반지름의 약 1.5배)
    float checkRadius = monster->GetRadius() * 2.5f;
    auto neighbors = room->GetMonstersInRange(mx, my, checkRadius);

    float sepX = 0, sepY = 0;
    int sepCount = 0;
    bool blockedAhead = false;

    for (auto &neighbor : neighbors)
    {
        if (neighbor.get() == monster)
            continue;

        float ox = neighbor->GetX() - mx;
        float oy = neighbor->GetY() - my;
        float d2 = ox * ox + oy * oy;
        float rSum = monster->GetRadius() + neighbor->GetRadius();

        if (d2 < rSum * rSum * 1.21f && d2 > 0.001f)
        {
            float d = std::sqrt(d2);
            // 분리 로직: 너무 가까우면 반대로 밀어냄
            sepX -= (ox / d) * (rSum - d);
            sepY -= (oy / d) * (rSum - d);
            sepCount++;

            // 정면(이동 방향 +-30도)에 장애물이 있는지 체크
            float dot = (ox / d) * nx + (oy / d) * ny;
            if (dot > 0.866f) // cos(30도)
            {
                blockedAhead = true;
            }
        }
    }

    float speed = monster->GetSpeed();

    if (blockedAhead)
    {
        // [Fluid Flow] 앞이 막혔을 때 멈추지 않고 옆으로 "흐르기"
        // 법선 벡터(90도 방향) 계산
        float lx = -ny;
        float ly = nx;
        float rx = ny;
        float ry = -nx;

        // 주변 밀어내는 힘(sepX, sepY)을 참고하여 저항이 적은 쪽 선택
        float leftForce = sepX * lx + sepY * ly;
        float rightForce = sepX * rx + sepY * ry;

        float flowX, flowY;
        if (leftForce > rightForce)
        {
            flowX = lx;
            flowY = ly;
        }
        else
        {
            flowX = rx;
            flowY = ry;
        }

        // 전방 성분은 줄이고 측면 성분을 강화하여 미끄러지듯 이동
        nx = nx * 0.2f + flowX * 0.8f;
        ny = ny * 0.2f + flowY * 0.8f;

        // 흐를 때는 부드러운 움직임을 위해 속도를 약간만 조절
        speed *= 0.9f;
    }
    else if (sepCount > 0)
    {
        // 일반적인 분리 로직 (유연하게 섞음)
        nx = nx * 0.6f + (sepX / sepCount) * 0.4f;
        ny = ny * 0.6f + (sepY / sepCount) * 0.4f;
    }

    // 최종 벡터 정규화
    float finalLen = std::sqrt(nx * nx + ny * ny);
    if (finalLen > 0.001f)
    {
        nx /= finalLen;
        ny /= finalLen;
    }

    // [Inertia] 관성 부여: 이전 속도와 부드럽게 보간하여 떨림 상쇄
    float curVx = monster->GetVX();
    float curVy = monster->GetVY();

    float targetVx = nx * speed;
    float targetVy = ny * speed;

    // 90% 관성 유지, 10% 신규 반영 (물처럼 부드러운 흐름 유도)
    outVx = curVx * 0.9f + targetVx * 0.1f;
    outVy = curVy * 0.9f + targetVy * 0.1f;
}

} // namespace SimpleGame
