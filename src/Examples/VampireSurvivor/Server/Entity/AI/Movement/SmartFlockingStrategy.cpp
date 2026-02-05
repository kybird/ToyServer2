#include "SmartFlockingStrategy.h"
#include "Entity/Monster.h"
#include "Game/Room.h"
#include "System/Utility/FastRandom.h"

namespace SimpleGame {

void SmartFlockingStrategy::CalculateMovement(
    Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
)
{
    (void)dt;

    // 1. Calculate Chase Vector
    float dx = targetX - monster->GetX();
    float dy = targetY - monster->GetY();
    float distSq = dx * dx + dy * dy;
    float dist = std::sqrt(distSq);

    if (dist <= 1.1f)
    {
        outVx = 0;
        outVy = 0;
        return;
    }

    float nx = dx / dist;
    float ny = dy / dist;
    float speed = monster->GetSpeed();

    // 2. Look-Ahead & Separation
    float sepX = 0, sepY = 0;
    int sepCount = 0;
    float maxOverlap = 0.0f;
    bool blockedAhead = false;

    // Look-ahead position (Next step)
    float lookAheadX = monster->GetX() + nx * 0.5f;
    float lookAheadY = monster->GetY() + ny * 0.5f;

    if (room)
    {
        auto neighbors = room->GetMonstersInRange(monster->GetX(), monster->GetY(), 1.5f);
        for (auto &n : neighbors)
        {
            if (n->GetId() == monster->GetId())
                continue;

            float nX = n->GetX();
            float nY = n->GetY();

            // [Separation] Check current overlap
            float ddx = monster->GetX() - nX;
            float ddy = monster->GetY() - nY;
            float lSq = ddx * ddx + ddy * ddy;
            float minSep = 1.2f;

            if (lSq < minSep * minSep)
            {
                float l = std::sqrt(lSq);
                float overlap = minSep - l;
                if (overlap > maxOverlap)
                    maxOverlap = overlap;

                if (l < 0.001f)
                {
                    static thread_local System::Utility::FastRandom rng;
                    sepX += rng.NextFloat(-1.0f, 1.0f);
                    sepY += rng.NextFloat(-1.0f, 1.0f);
                }
                else
                {
                    float weight = overlap / minSep;
                    sepX += (ddx / l) * weight * 2.5f; // Stronger push
                    sepY += (ddy / l) * weight * 2.5f;
                }
                sepCount++;
            }

            // [Look-Ahead] Check future blocking (only if not already deeply overlapping)
            if (maxOverlap < 0.1f)
            {
                float fdx = lookAheadX - nX;
                float fdy = lookAheadY - nY;
                if (fdx * fdx + fdy * fdy < 1.0f) // If my future head hits their body
                {
                    blockedAhead = true;
                }
            }
        }
    }

    // 3. Resolve
    if (sepCount > 0 && maxOverlap > 0.1f)
    {
        // [EMERGENCY] Deep overlap -> Force Separation
        nx = sepX;
        ny = sepY;
        if (maxOverlap > 0.3f)
            speed *= 1.5f;

        float newL = std::sqrt(nx * nx + ny * ny);
        if (newL > 0.001f)
        {
            nx /= newL;
            ny /= newL;
        }
    }
    else if (blockedAhead)
    {
        // 전방이 막혔을 때 처음 0.8초간은 이동을 멈추고 관찰 (인내심 구간)
        if (monster->GetStuckTimer() <= 0.0f)
        {
            monster->SetStuckTimer(0.8f);
        }

        if (monster->GetStuckTimer() > 0.3f)
        {
            // 정지 상태 유지
            nx = 0;
            ny = 0;
            speed = 0;
        }
        else
        {
            // [AVOIDANCE] 충분히 기다렸다면 좌우 우회로 탐색
            float sideX1 = -ny;
            float sideY1 = nx;
            float sideX2 = ny;
            float sideY2 = -nx;

            bool useFirst = (sepX * sideX1 + sepY * sideY1) > (sepX * sideX2 + sepY * sideY2);

            if (useFirst)
            {
                nx = nx * 0.3f + sideX1 * 0.7f;
                ny = ny * 0.3f + sideY1 * 0.7f;
            }
            else
            {
                nx = nx * 0.3f + sideX2 * 0.7f;
                ny = ny * 0.3f + sideY2 * 0.7f;
            }

            speed *= 0.8f;

            float newL = std::sqrt(nx * nx + ny * ny);
            if (newL > 0.001f)
            {
                nx /= newL;
                ny /= newL;
            }
        }
    }
    else
    {
        // 막히지 않았다면 타이머 초기화 (나중에 다시 막혔을 때 인내심을 처음부터 발동하기 위해)
        monster->SetStuckTimer(0.0f);

        if (sepCount > 0)
        {
            // Minor separation blending
            float sepAvgX = sepX / sepCount;
            float sepAvgY = sepY / sepCount;

            nx = nx * 0.5f + sepAvgX * 0.5f;
            ny = ny * 0.5f + sepAvgY * 0.5f;

            float newL = std::sqrt(nx * nx + ny * ny);
            if (newL > 0.001f)
            {
                nx /= newL;
                ny /= newL;
            }
        }
    }

    outVx = nx * speed;
    outVy = ny * speed;
}

} // namespace SimpleGame
