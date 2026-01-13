#pragma once

namespace SimpleGame {

struct GameConfig
{
    static constexpr int TPS = 25;
    static constexpr int TICK_INTERVAL_MS = 1000 / TPS;
    static constexpr float TICK_INTERVAL_SEC = 1.0f / static_cast<float>(TPS);

    // Combat settings
    static constexpr float KNOCKBACK_FORCE = 3.0f;    // 넉백 속도 (캐릭터 크기 1x1에 맞게 조정)
    static constexpr float KNOCKBACK_DURATION = 0.2f; // 넉백 지속시간 (초)

    // Collision settings (Lag compensation: Visual size 0.5, Collision size 0.2)
    static constexpr float PLAYER_COLLISION_RADIUS = 0.5f;  // 플레이어 충돌 반지름
    static constexpr float MONSTER_COLLISION_RADIUS = 0.5f; // 몬스터 충돌 반지름
    // static constexpr float AI_STOP_OVERLAP_DIST = 0.1f;    // REMOVED

    // Combat Advanced Settings
    static constexpr float AI_STOP_MARGIN = 0.1f;             // 몬스터가 플레이어 앞에서 멈추는 거리 (겹침 방지)
    static constexpr float MONSTER_ATTACK_REACH = 0.2f;       // 몬스터 공격 사거리 (몸체 밖으로 뻗는 거리)
    static constexpr float PLAYER_INVINCIBLE_DURATION = 0.5f; // 피격 시 무적 시간 (초)
};

} // namespace SimpleGame
