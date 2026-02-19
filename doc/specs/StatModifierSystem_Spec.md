# 범용 Stat Modifier 시스템 구현 명세 (Stat Modifier System Spec) - [Status: IMPLEMENTED]

**구현 위치**: `src/Examples/VampireSurvivor/Server/Entity/StatModifier.h`

몬스터와 플레이어의 스탯(이동속도, 공격력 등)을 특정 기능에 종속시키지 않고, 범용적으로 관리할 수 있는 **Stat Modifier 시스템**을 설계합니다.

## 1. 개요
현재 `Monster`의 `_levelUpSlowCount`와 같이 특정 스킬에 특화된 변수를 제거하고, 모든 능력치 변화를 `Modifier` 객체들로 관리합니다. 이는 MMORPG에서 수십 개의 버프/디버프가 중첩되는 상황을 효율적이고 견고하게 처리하기 위함입니다.

## 2. 주요 설계 구성 요소

### A. `StatModifier` 구조 (Base)
다양한 연산 방식을 지원하는 기본 구조체입니다.
- **연산 방식 (ModifierType)**:
  - `Flat`: 고정 수치 추가 (예: 이동 속도 +1.0)
  - `PercentAdd`: 기본 수치에 대한 백분율 합산 (예: 공격력 +10%)
  - `PercentMult`: 최종 수치에 대한 곱산 (예: 이동 속도 x0.1)
- **효과 식별자 (SourceId)**: 어떤 스킬이나 오브젝트로부터 온 효과인지 식별. 중복 적용 방지 및 수동 해제 시 사용.
- **만료 시간 (ExpirationTime)**: 효과가 지속되는 시간. 0일 경우 영구 또는 수동 해제 전까지 유지.

### B. `ModifierContainer` 클래스
각 `GameObject`가 가지는 스탯 관리 컴포넌트입니다.
- `std::vector<StatModifier>`를 관리하며, 스탯 요청 시 최종 값을 계산합니다.
- **최종 값 계산 공식**: `(Base + FlatSum) * (1.0 + PercentAddSum) * (PercentMultProduct)`
- 효율성을 위해 `Dirty Flag`를 사용하여 값이 변할 때만 재계산하거나, 계산 결과를 캐싱합니다.

### C. 데이터 테이블 확장 (`SkillTemplate`)
스킬 데이터에서 효과를 정의할 수 있도록 항목을 구체화합니다. (기존 과제 반영)
- `EffectType`: `None`, `Slow`, `SpeedBuff`, `Haste`, `Poison` 등 (Enum화)
- `OpType`: `Flat`, `PercentAdd`, `PercentMult`
- `Value`: 효과 수치
- `Duration`: 지속 시간

## 3. 상세 구현 단계

### [Step 1] 시스템 기반 구축
1. `src/Examples/VampireSurvivor/Server/Entity/StatModifier.h` 생성.
2. `ModifierContainer` 클래스 구현 (계산 로직 및 Dirty flag 포함).
3. `GameObject`가 `ModifierContainer`를 멤버로 가지도록 통합.

### [Step 2] 엔티티 속도 시스템 통합
1. `Monster::GetSpeed()`가 하드코딩된 변수(`_speedMultiplier`) 대신 `ModifierContainer`의 결과를 반환하도록 수정.
2. `Player::GetMovementSpeed()` 등 다른 스탯도 순차적으로 통합 고려.
3. `MonsterFactory`에서 초기 스탯 설정 시 Base값으로 주입.

### [Step 3] 레벨업 슬로우 기능 재구현
1. `Player::Update`의 오라 감지 로직 수정:
   - 오라 진입 시: `ModifierContainer::AddModifier(SlowEffect)` 호출.
   - 오라 이탈 시: `ModifierContainer::RemoveModifier(SourceId)` 호출.
2. 이 구조를 통해 추후 다른 투박한 '상태 변화' 로직들을 이 시스템으로 흡수.

## 4. 기대 효과 및 MMORPG 지향점
- **확장성**: 새로운 버프/디버프 추가 시 엔티티 코드를 수정할 필요가 없음.
- **정확성**: 수치 누적 계산 시 발생하는 부동소수점 오차나 누수 문제를 시스템 레벨에서 방지.
- **유연성**: 단순 슬로우뿐만 아니라 중복 적용, 가장 강한 효과만 적용 등 복잡한 비즈니스 로직 대응 가능.

---
**작성일**: 2026-01-17
**담당**: Antigravity (AI Assistant)
