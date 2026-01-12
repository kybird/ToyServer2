# 전투 시스템 구현 요구사항 (Combat System Requirements)

> 📅 작성일: 2026-01-12
> 📂 기반 문서: `spec.md`, `GameCompletionPlan.md`, `todo.md`

## 1. 개요 (Overview)
전투 시스템은 `뱀파이어 서바이버` 클론 프로젝트의 핵심 게임 루프입니다. 플레이어의 자동 공격, 몬스터와의 충돌 처리, 투사체 관리, 그리고 사망/부활 로직을 포함합니다.
모든 전투 로직은 **서버 권위(Server-Authoritative)** 모델을 따르며, 클라이언트는 서버의 상태를 시각화하는 역할만 수행합니다.

## 2. 핵심 기능 요구사항 (Core Features)

### 2.1 투사체 시스템 (Projectile System)
- **생성**: `S_SpawnObject` (Type: PROJECTILE) 패킷 사용.
- **이동**: 서버에서 위치 및 궤적 계산. 동기화 빈도 최적화 필요.
- **충돌**: 서버에서 `Projectile` vs `Monster` 충돌 판정 (SpatialGrid 활용).
- **소멸**: 최대 사거리 도달, 관통 횟수 소진, 또는 맵 밖으로 이동 시 `S_DespawnObject`.
- **최적화**: 화면 당 최대 투사체 수 제한 (예: 100개).

### 2.2 플레이어 공격 (Player Attack)
- **자동 공격**: 스킬별 쿨타임(Cooldown)에 따라 서버가 자동으로 투사체 생성.
- **스킬 로직**:
    - **Active**: 무기 (Whip, Knife, Axe 등).
    - **Passive**: 스탯 강화 (Spinach, Armor 등).
- **데이터 기반**: `SkillData.json`에서 데미지, 쿨타임, 투사체 속도, 관통 수 등을 로드.

### 2.3 몬스터 전투 (Monster Combat)
- **몸체 박치기 (Body Collision)**:
    - 몬스터와 플레이어의 충돌체(Circle)가 겹칠 경우 데미지 적용.
    - 몬스터별 공격 쿨타임 적용 (매 프레임 데미지 방지).
    - `S_DamageEffect` 패킷으로 데미지 수치 및 피격 효과 전송.
    - **[New] 넉백상태 (Knockback)**: 몬스터가 플레이어를 가격하면, 몬스터가 플레이어 반대 방향으로 튕겨나감 (겹침 방지 및 타격감).
- **사망 처리**:
    - HP <= 0 시 `DEAD` 상태 전환.
    - `S_DespawnObject` 전송.
    - 확률적으로 `ExpGem` 또는 `Item` 드랍.

### 2.4 데미지 및 상태 이상 (Damage & Status)
- **넉백 (Knockback)**: 피격 시 반대 방향으로 밀려남.
- **피격 무적**: 플레이어 피격 시 짧은 무적 시간 부여 (선택 사항).

## 3. 구현 상세 (Implementation Details)

### 3.1 신규/수정 클래스
- ** CombatManager (신규) **
    - 역할: 투사체와 몬스터 간의 충돌 검사 및 처리 총괄.
    - 주요 메소드: `Update(dt)`, `ResolveCollisions()`.
    
- ** Projectile (Entity 확장) **
    - 기존 `GameObject` 상속.
    - 속성: `Damage`, `Penetration`, `Duration`, `OwnerId`.
    
- ** SkillComponent (Player 내부 또는 별도) **
    - 플레이어의 보유 스킬 및 쿨타임 관리.
    - `Update(dt)`에서 쿨타임 체크 후 `CombatManager`에 발사 요청.

### 3.2 패킷 프로토콜 (Protocol)
- `S_SpawnProjectile`: 투사체 생성 알림.
- `S_DamageEffect`: 데미지 입음 (TargetId, DamageAmount, IsCritical).
- `S_DespawnObject`: 객체 제거 (사망 등).
- `S_ExpChange`: 경험치 획득 (선행 작업).

### 3.3 저장 데이터 (Data Structure)
- **SkillData.json**:
  ```json
  {
    "id": 101,
    "name": "Magic Wand",
    "type": "ACTIVE",
    "damage": 10,
    "cooldown": 1.0,
    "projectile_speed": 10.0,
    "max_penetration": 1
  }
  ```

## 4. 우선순위 (Priority) - P2: MVP
1. **투사체 생성 및 이동**: `Magic Wand` (가장 가까운 적 자동 공격) 구현.
2. **충돌 시스템**: 투사체 -> 몬스터 히트 & 데미지.
3. **몬스터 -> 플레이어 피격**: 몸체 박치기 데미지.
4. **몬스터 사망 및 제거**: HP 0 처리.

---
**다음 단계 제안**: `CombatManager` 클래스 생성 및 기본 투사체 로직 구현.
