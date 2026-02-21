#include "StrictSeparationStrategy.h"
#include "Entity/Monster.h"
#include "Game/Room.h"
#include "System/Utility/FastRandom.h"

namespace SimpleGame::Movement {

void StrictSeparationStrategy::CalculateMovement(
    Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
)
{
    (void)dt;

    // 1. 추적 벡터 계산 (기본 목표)
    float dx = targetX - monster->GetX();
    float dy = targetY - monster->GetY();
    float distSq = dx * dx + dy * dy;
    float dist = std::sqrt(distSq);

    // 목표 도달 시 정지 (플레이어 범위와의 단순 충돌 처리)
    // 가정: 플레이어 반지름 0.5 + 몬스터 0.5 + 여유 공간 0.1 = 1.1
    if (dist <= 1.1f)
    {
        outVx = 0;
        outVy = 0;
        return;
    }

    // 정규화된 추적 방향
    float nx = dx / dist;
    float ny = dy / dist;

    // 2. 엄격한 거리두기(분리) 점검
    float sepX = 0, sepY = 0;
    int sepCount = 0;
    float maxOverlap = 0.0f;

    if (room)
    {
        auto neighbors = room->GetMonstersInRange(monster->GetX(), monster->GetY(), 1.2f);
        for (auto &n : neighbors)
        {
            if (n->GetId() == monster->GetId())
                continue;

            float ddx = monster->GetX() - n->GetX();
            float ddy = monster->GetY() - n->GetY();
            float lSq = ddx * ddx + ddy * ddy;
            float minSep = 1.2f; // 균형 잡힌 간격

            if (lSq < minSep * minSep)
            {
                float l = std::sqrt(lSq);
                float overlap = minSep - l;
                if (overlap > maxOverlap)
                    maxOverlap = overlap;

                if (l < 0.001f)
                {
                    // 완벽히 겹쳤을 때를 대비한 무작위 위치 조작
                    static thread_local System::Utility::FastRandom rng;
                    sepX += rng.NextFloat(-1.0f, 1.0f);
                    sepY += rng.NextFloat(-1.0f, 1.0f);
                }
                else
                {
                    float weight = overlap / minSep;
                    sepX += (ddx / l) * weight * 2.0f;
                    sepY += (ddy / l) * weight * 2.0f;
                }
                sepCount++;
            }
        }
    }

    float speed = monster->GetSpeed();

    // 3. 최종 벡터 확정
    if (sepCount > 0 && maxOverlap > 0.1f)
    {
        // [위기상황] 추적 정지, 오직 밀어내는 동작만 수행
        nx = sepX;
        ny = sepY;

        // 긴급상황 탈출 속도 적용
        if (maxOverlap > 0.3f)
            speed *= 1.5f;

        float newL = std::sqrt(nx * nx + ny * ny);
        if (newL > 0.001f)
        {
            nx /= newL;
            ny /= newL;
        }
    }
    else if (sepCount > 0)
    {
        // 약한 분리 혼합 (블렌딩)
        float sepAvgX = sepX / sepCount;
        float sepAvgY = sepY / sepCount;

        nx = nx * 0.6f + sepAvgX * 0.4f;
        ny = ny * 0.6f + sepAvgY * 0.4f;

        float newL = std::sqrt(nx * nx + ny * ny);
        if (newL > 0.001f)
        {
            nx /= newL;
            ny /= newL;
        }
    }

    outVx = nx * speed;
    outVy = ny * speed;
}

} // namespace SimpleGame::Movement
