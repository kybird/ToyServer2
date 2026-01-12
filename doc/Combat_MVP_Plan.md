# ⚔️ 전투 시스템 MVP 구축 계획 (Combat MVP Plan)

> 📅 작성일: 2026-01-12
> 🎯 목표: **"플레이 가능한 최소한의 전투 루프"** 완성

---

## 1. MVP 범위 (Scope)

### ✅ 포함 (Must Have)
| 기능 | 서버 | 클라 |
|------|:----:|:----:|
| 투사체 발사 (Magic Wand) | ✅ | ✅ |
| Projectile vs Monster 충돌 | ✅ | 시각화 |
| Monster vs Player 충돌 | ✅ | 시각화 |
| 넉백 (Knockback) | ✅ | 시각화 |

### ❌ 제외 (Next Phase)
- 경험치 젬 및 레벨업
- 다양한 스킬 및 진화
- 복잡한 이펙트 최적화

---

## 2. 서버 구현 (Server Implementation)

### 2.1 CombatManager 클래스 (신규)
**경로**: `Server/Game/CombatManager.h/cpp`

```cpp
class CombatManager {
public:
    void Update(float dt, Room* room);
private:
    void ResolveProjectileCollisions(Room* room);
    void ResolveBodyCollisions(Room* room);
    void ApplyKnockback(Monster* monster, float dirX, float dirY);
};
```

### 2.2 서버 작업 목록
- [x] **[S1] CombatManager 생성**: `Room.cpp` 충돌 로직 분리
- [x] **[S2] Projectile 충돌**: 투사체 → 몬스터 데미지 & Despawn
- [x] **[S3] Body 충돌**: 몬스터 → 플레이어 데미지 & HP 패킷
- [x] **[S4] 넉백 적용**: 충돌 시 몬스터 속도 반전
    - `Monster::ApplyKnockback(float force, float dirX, float dirY)`
- [x] **[S5] 패킷 전송**:
    - `S_DamageEffect`: 데미지 수치
    - `S_HpChange`: HP 변경
    - `S_MoveObjectBatch`: 넉백 후 위치 동기화

### 2.3 넉백 로직 (Knockback)
```cpp
// CombatManager::ApplyKnockback
float dx = monster->GetX() - player->GetX();
float dy = monster->GetY() - player->GetY();
float len = sqrt(dx*dx + dy*dy);
if (len > 0) {
    dx /= len; dy /= len;
    monster->SetVelocity(dx * KNOCKBACK_FORCE, dy * KNOCKBACK_FORCE);
}
```

---

## 3. 클라이언트 구현 (Client Implementation - Unity)

### 3.1 클라이언트 작업 목록
- [x] **[C1] 투사체 시각화**: `S_SpawnObject(PROJECTILE)` 수신 시 프리팹 생성
- [x] **[C2] 투사체 이동**: Dead Reckoning 또는 `S_MoveObjectBatch` 보간
- [x] **[C3] 투사체 소멸**: `S_DespawnObject` 수신 시 제거 + 히트 이펙트
- [x] **[C4] 데미지 표시**: `S_DamageEffect` 수신 시 데미지 숫자 팝업
- [x] **[C5] HP 바 업데이트**: `S_HpChange` 수신 시 UI 갱신
- [x] **[C6] 넉백 시각화**: 몬스터 위치 보간 (이미 `S_MoveObjectBatch`로 처리됨)

### 3.2 필요 프리팹/에셋
- `Projectile_MagicWand.prefab`: 투사체 비주얼
- `DamagePopup.prefab`: 데미지 숫자 UI
- `HitEffect.prefab`: 타격 이펙트 (선택)

---

## 4. 검증 (Verification)

### 서버 테스트
- [x] `TestCombat` 유닛 테스트에 넉백 시나리오 추가
- [x] 로그 확인: `[Combat] Monster(101) knocked back from Player(1)`

### 클라이언트 테스트
- [x] 투사체가 몬스터에 맞으면 사라지는지 확인
- [ ] 몬스터가 플레이어와 부딪히면 튕겨나가는지 확인
- [x] HP 바가 정상적으로 감소하는지 확인

---

**예상 소요 시간**: 서버 0.5일 + 클라 0.5일 = 1일
**완료 기준**: 플레이어가 가만히 있어도 몬스터를 죽이고, 부딪히면 튕겨나가는 것을 **게임 화면에서** 확인.
