#include "StrictSeparationStrategy.h"
#include "Entity/Monster.h"
#include "Game/Room.h"
#include "System/Utility/FastRandom.h"

namespace SimpleGame {

void StrictSeparationStrategy::CalculateMovement(
    Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
)
{
    (void)dt;

    // 1. Calculate Chase Vector (Base Intent)
    float dx = targetX - monster->GetX();
    float dy = targetY - monster->GetY();
    float distSq = dx * dx + dy * dy;
    float dist = std::sqrt(distSq);

    // Stop if reached target (Simple collision with player radius)
    // Assuming player radius 0.5 + Monster 0.5 + Margin 0.1 = 1.1
    if (dist <= 1.1f)
    {
        outVx = 0;
        outVy = 0;
        return;
    }

    // Normalized chase direction
    float nx = dx / dist;
    float ny = dy / dist;

    // 2. Strict Separation Check
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
            float minSep = 1.2f; // Balanced Gap

            if (lSq < minSep * minSep)
            {
                float l = std::sqrt(lSq);
                float overlap = minSep - l;
                if (overlap > maxOverlap)
                    maxOverlap = overlap;

                if (l < 0.001f)
                {
                    // Random nudge for perfect overlap
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

    // 3. Resolve Intent
    if (sepCount > 0 && maxOverlap > 0.1f)
    {
        // [CRITICAL] Stop chasing, pure separation
        nx = sepX;
        ny = sepY;

        // Emergency escape speed
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
        // Minor blend
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

} // namespace SimpleGame
