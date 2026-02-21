#include "VampireSurvivorMovementStrategy.h"
#include "Entity/Monster.h"
#include "Game/Room.h"
#include <cmath>

namespace SimpleGame {

void VampireSurvivorMovementStrategy::CalculateMovement(
    Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
)
{
    if (!monster || !room)
        return;

    // 1. 플레이어를 향한 방향 벡터 계산 (직진 로직)
    float currentX = monster->GetX();
    float currentY = monster->GetY();

    float dx = targetX - currentX;
    float dy = targetY - currentY;

    float distanceSq = dx * dx + dy * dy;

    // 타겟과 이미 매우 가까우면 제자리 유지
    if (distanceSq < 0.001f)
    {
        outVx = 0.0f;
        outVy = 0.0f;
        return;
    }

    float distance = std::sqrt(distanceSq);
    float dirX = dx / distance;
    float dirY = dy / distance;

    // 기본 이동 속도
    float speed = monster->GetSpeed();
    float baseVx = dirX * speed;
    float baseVy = dirY * speed;

    // 2. 다른 몬스터들과의 겹침 밀어내기 (Soft Collision)
    float pushVx = 0.0f;
    float pushVy = 0.0f;

    // 충돌 처리 시 실제 몬스터 반지름보다 훨씬 큰 가상의 반지름을 사용하여 넓게 퍼지도록 만듭니다. (약 3배)
    float separationRadius = monster->GetRadius() * 3.0f;
    float searchRadius = separationRadius * 2.0f;
    auto neighbors = room->GetMonstersInRange(currentX, currentY, searchRadius);

    for (const auto &neighbor : neighbors)
    {
        if (neighbor.get() == monster || neighbor->IsDead())
            continue;

        float nDx = currentX - neighbor->GetX();
        float nDy = currentY - neighbor->GetY();
        float nDistSq = nDx * nDx + nDy * nDy;

        float nSeparationRadius = neighbor->GetRadius() * 3.0f;
        float minDistance = separationRadius + nSeparationRadius;

        // 겹쳐있거나 너무 가까운 경우 밀어내는 힘 발생
        if (nDistSq > 0.0001f && nDistSq < minDistance * minDistance)
        {
            float nDist = std::sqrt(nDistSq);
            // 겹친 정도에 비례하여 밀어내는 힘 계산 (선형)
            float pushStrength = (minDistance - nDist) / minDistance;

            // 밀어내는 방향 정규화 및 크기 적용
            nDx /= nDist;
            nDy /= nDist;

            // pushStrength에 상수를 곱해 밀어내는 세기 조절 (거리가 멀어졌으므로 힘도 적당히 조절)
            float pushFactor = speed * 2.0f;
            pushVx += nDx * pushStrength * pushFactor;
            pushVy += nDy * pushStrength * pushFactor;
        }
    }

    // 3. 최종 속도 합성 및 정규화 (최대 속도 초과 방지)
    outVx = baseVx + pushVx;
    outVy = baseVy + pushVy;

    float finalDistSq = outVx * outVx + outVy * outVy;
    if (finalDistSq > speed * speed)
    {
        float finalDist = std::sqrt(finalDistSq);
        outVx = (outVx / finalDist) * speed;
        outVy = (outVy / finalDist) * speed;
    }
}

} // namespace SimpleGame
