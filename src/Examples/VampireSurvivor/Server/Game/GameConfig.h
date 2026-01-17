#pragma once

namespace SimpleGame {

struct GameConfig
{
    static constexpr int TPS = 25;
    static constexpr int TICK_INTERVAL_MS = 1000 / TPS;
    static constexpr float TICK_INTERVAL_SEC = 1.0f / static_cast<float>(TPS);
    static constexpr int DEFAULT_ROOM_ID = 1;

    // Combat settings
    static constexpr float KNOCKBACK_FORCE = 3.0f;    // 넉백 속도 (캐릭터 크기 1x1에 맞게 조정)
    static constexpr float KNOCKBACK_DURATION = 0.2f; // 넉백 지속시간 (초)

    // Collision settings (Lag compensation: Visual size 0.5, Collision size 0.2)
    static constexpr float PLAYER_COLLISION_RADIUS = 0.5f;  // 플레이어 충돌 반지름
    static constexpr float MONSTER_COLLISION_RADIUS = 0.5f; // 몬스터 충돌 반지름

    // SpatialGrid settings
    static constexpr float NEAR_GRID_CELL_SIZE = 2.0f;
    static constexpr float FAR_GRID_CELL_SIZE = 10.0f;
    static constexpr float FAR_QUERY_THRESHOLD = 8.0f; // radii >= this use far grid

    // static constexpr float AI_STOP_OVERLAP_DIST = 0.1f;    // REMOVED

    // Combat Advanced Settings
    static constexpr float AI_STOP_MARGIN = 0.1f;             // 몬스터가 플레이어 앞에서 멈추는 거리 (겹침 방지)
    static constexpr float MONSTER_ATTACK_REACH = 0.2f;       // 몬스터 공격 사거리 (몸체 밖으로 뻗는 거리)
    static constexpr float PLAYER_INVINCIBLE_DURATION = 0.5f; // 피격 시 무적 시간 (초)

    // Wave/Spawn Settings
    static constexpr float MONSTER_SPAWN_CLUSTER_RADIUS = 20.0f; // 플레이어 그룹화 반경
    static constexpr float MONSTER_SPAWN_MIN_DIST = 30.0f;       // 그룹 중심으로부터 최소 스폰 거리
    static constexpr float MONSTER_SPAWN_MAX_DIST = 35.0f;       // 그룹 중심으로부터 최대 스폰 거리

    // Level Up Settings
    static constexpr int32_t EXP_BASE = 100;
    static constexpr int32_t EXP_PER_LEVEL_INCREMENT = 50;
    static constexpr float LEVEL_UP_TIMEOUT_SEC = 30.0f;
};

} // namespace SimpleGame
