#pragma once
#include "GameConfig.h"
#include <memory>
#include <vector>

namespace SimpleGame {

class Room;
class Monster;
class Player;
class Projectile;
class ObjectManager;
class SpatialGrid;

/**
 * @brief 몬스터 공격 이벤트 (충돌 판정 결과)
 *
 * Pass 1에서 수집되어 Pass 2에서 처리됩니다.
 * 순수 데이터 구조로 SIMD 최적화에 적합합니다.
 */
struct AttackEvent
{
    size_t monsterIndex; // 공격한 몬스터의 인덱스 (DOD 모드)
    int32_t monsterId;   // 공격한 몬스터의 ID (Legacy 모드)
    int32_t playerId;    // 피해를 입은 플레이어 ID
    int32_t damage;      // 적용할 데미지
    float attackTime;    // 공격 시각 (쿨다운 갱신용)
};

class CombatManager
{
public:
    CombatManager();
    ~CombatManager();

    void Update(float dt, Room *room);

private:
    void ResolveProjectileCollisions(float dt, Room *room);
    void ResolveBodyCollisions(float dt, Room *room);
    void ResolveItemCollisions(float dt, Room *room);
    void ResolveCleanup(Room *room);

    // 2-Pass 충돌 처리 (SIMD 최적화 준비)
    void CollectAttackEvents(Room *room, std::vector<AttackEvent> &outEvents);
    void ExecuteAttackEvents(Room *room, const std::vector<AttackEvent> &events);

    // ApplyKnockback removed per user request
    // void ApplyKnockback(std::shared_ptr<Monster> monster, std::shared_ptr<Player> player, Room *room);

private:
    std::vector<AttackEvent> attackEventBuffer_; // 재할당 방지용 버퍼
};

} // namespace SimpleGame
