# 무기 및 패시브 시스템 아키텍처 (Skills.md)

이 문서는 ToyServer2의 **특성 기반(Trait-based) 글로벌 패시브 시스템** 및 현재 구현된 무기/기술 상세 정보를 설명합니다.

---

## 1. 개요 (Overview)
- **무기(Weapon)**: `DamageEmitter`를 통해 로직이 처리되며, 레벨업 시 데미지/쿨다운 배율이 강화됩니다.
- **기술 특성(Skill Traits)**: 모든 기술은 고유의 특성을 가집니다. 패시브는 이 특성을 보고 적용 대상을 결정합니다.
- **글로벌 패시브(Global Passives)**: 뱀파이어 서바이벌 방식으로, 하나의 패시브가 해당 특성을 가진 모든 무기를 동시에 강화합니다.

---

## 2. 무기 및 기술 리스트 (Weapons & Skills)

| ID | 이름 | 공격 방식 (Emitter) | 특성 (Traits) | 주요 효과 |
|:---|:---|:---|:---|:---|
| 1 | **Basic Dagger** | Linear | `PROJECTILE`, `PIERCE`, `LINEAR` | 가장 가까운 적에게 단검 발사 |
| 2 | **Poison Dart** | Linear | `PROJECTILE`, `PIERCE`, `LINEAR`, `EFFECT` | 적중 시 중독(POISON) 데미지 |
| 3 | **Frost Nova** | AoE (Field) | `AOE`, `AREA`, `DURATION`, `EFFECT` | 주변 적 슬로우 및 지속 데미지 |

### 기술별 상세 스펙 (Level 1 기준)
- **Dagger**: 데미지 10, 쿨다운 1.0s, 투사체 속도 15.0
- **Poison Dart**: 데미지 5, 중독 데미지 5(3초간), 쿨다운 1.5s
- **Frost Nova**: 데미지 5(0.5s 간격), 반경 3.0, 쿨다운 6.0s, 지속시간 3.0s, 슬로우 40%

---

## 3. 글로벌 패시브 시스템 (Passives)

| 패시브 이름 | 적용 스탯 (statType) | 관련 특성 | 효과 |
|:---|:---|:---|:---|
| **Spinach** | `damage` | (전체) | 모든 무기 공격력 증가 |
| **Empty Tome** | `cooldown` | (전체) | 모든 무기 재사용 대기시간 감소 |
| **Duplicator** | `projectile_count` | `PROJECTILE` | 투사체 발사 개수 증가 |
| **Magic Bindi** | `pierce` | `PROJECTILE` | 투사체 관통 횟수 증가 |
| **Candelabrador** | `area` | `AREA`, `AOE` | 공격 범위/폭발 반경 증가 |
| **Spellbinder** | `duration` | `DURATION` | 지속 시간(장판 등) 증가 |
| **Wings** | `speed` | (캐릭터) | 플레이어 이동 속도 증가 |
| **Hollow Heart** | `max_hp` | (캐릭터) | 플레이어 최대 체력 % 증가 |

---

## 4. 작동 원리 (Mechanics)

1.  **데이터 정의**: `PlayerBaseSkill.json`에서 기술의 `traits`를 정의합니다.
2.  **보너스 계산**: `Player` 클래스의 `GetXXXMultiplier()` 메서드가 인벤토리를 순회하며 글로벌 보너스를 합계합니다.
3.  **최종 적용**: `DamageEmitter`가 실행될 때, 자신의 `traits` 리스트에 해당 스탯과 관련된 키워드가 있는지 확인하고 보너스를 적용합니다.
    - *예: Frost Nova는 `AREA` 특성이 있으므로 `Candelabrador`의 범위를 적용받지만, `Duplicator(투사체)`의 영향은 받지 않습니다.*

---

## 5. 향후 확장성

- **신규 무기**: 새로운 무기를 추가할 때 기존 특성(`PROJECTILE` 등)을 부여하기만 하면 기존 패시브들과 즉시 연동됩니다.
- **카테고리 확장**: 필요 시 `KNOCKBACK`, `CRITICAL` 등의 새로운 특성과 이를 강화하는 패시브를 쉽게 추가할 수 있습니다.

---
*마지막 업데이트: 2026-02-05*
