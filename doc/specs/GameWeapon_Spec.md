# 무기 및 기술 시스템 명세 (Game Weapon & Skills Spec) - [Status: ACTIVE]

**근거 데이터**: `Data/PlayerBaseSkill.json`
**최종 업데이트**: 2026-02-18

---

## 1. 개요 (Overview)
ToyServer2는 **DamageEmitter** 시스템을 기반으로 다양한 형태의 공격 방식과 **특성(Traits)** 시스템을 지원합니다.
모든 무기는 `emitter_type`에 따라 발사/공격 방식이 결정되며, `traits` 키워드를 통해 글로벌 패시브(아이템)의 효과를 적용받습니다.

---

## 2. 무기 목록 (Weapon List)

| ID | 이름 | Emitter Type | DMG | Cooldown | 주요 특징 | Traits (특성) |
|:---:|:---|:---|:---:|:---:|:---|:---|
| 1 | **Basic Dagger** | `Linear` | 10 | 1.0s | 가장 가까운 적에게 단검 발사 | `PROJECTILE`, `PIERCE`, `LINEAR` |
| 2 | **Poison Dart** | `Linear` | 5 | 1.5s | 적중 시 중독 효과 (3s) | `PROJECTILE`, `PIERCE`, `LINEAR`, `EFFECT` |
| 3 | **Frost Nova** | `AoE` | 5 | 6.0s | 주변 광역 빙결 (Slow 40%) | `AOE`, `AREA`, `DURATION`, `EFFECT` |
| 4 | **Greatsword** | `Arc` | 15 | 1.5s | 전방 부채꼴(30도) 베기 | `AOE`, `ARC`, `MELEE` |
| 5 | **King Bible** | `Orbit` | 8 | 2.0s | 캐릭터 주변을 공전하는 책 | `ORBIT`, `PROJECTILE`, `PIERCE` |
| 6 | **Lightning Ring** | `Zone` | 25 | 3.0s | 무작위 위치에 번개 낙하 | `ZONE`, `INSTANT`, `PROJECTILE`, `AREA` |
| 7 | **Whip** | `Directional` | 12 | 1.2s | 바라보는 방향으로 긴 채찍 | `DIRECTIONAL`, `MELEE`, `AREA` |
| 8 | **Garlic** | `Aura` | 3 | 1.0s | 캐릭터 주변 지속 피해 영역 | `AURA`, `AREA`, `DURATION` |

---

## 3. Emitter Types (발사 방식)

| 타입 | 설명 | 예시 |
|:---|:---|:---|
| **Linear** | 직선으로 날아가는 투사체. (속도: 15.0) | Dagger, Dart |
| **AoE** | 지정된 위치(또는 자신) 주변 원형 범위 타격. | Frost Nova |
| **Arc** | 부채꼴 범위 공격. (근접 공격 시뮬레이션) | Greatsword |
| **Orbit** | 캐릭터 중심의 일정 반경을 회전하며 이동. | King Bible |
| **Zone** | 화면 내 랜덤 위치 또는 특정 규칙에 따른 지점 타격. | Lightning Ring |
| **Directional** | 캐릭터가 바라보는 방향으로 고정된 형태(Rect/Circle)의 타격 판정 생성. | Whip |
| **Aura** | 캐릭터 중심에 항상 붙어다니는 영구적인 데미지 영역. | Garlic |

---

## 4. 특성 시스템 (Traits System)

글로벌 패시브 아이템은 아래 `Keywords`를 참조하여 보너스를 적용합니다.

- **PROJECTILE**: 투사체 개수 증가(`Duplicator`), 관통력 증가(`Magic Bindi`) 적용 대상. (예: `Linear`, `Orbit`)
- **AREA**: 공격 범위 및 폭발 반경 증가(`Candelabrador`) 적용 대상. (예: 마늘, 번개 반지)
- **DURATION**: 지속 시간 증가(`Spellbinder`) 적용 대상. (예: 성수, 비둘기)
- **MELEE**: 근접 공격 관련 보너스 적용 대상.
- **EFFECT**: 상태 이상(독, 슬로우 등) 효과 관련 보너스 적용 대상.

> **작동 원리**: `DamageEmitter::Update` 실행 시, 현재 무기가 가진 `traits` 배열(예: `["PROJECTILE", "PIERCE"]`)을 순회하며 대상 태그가 있는지 판단합니다. 만약 `"AREA"` 태그가 있다면, 플레이어의 글로벌 스탯인 `owner->GetAreaMultiplier()` 수치를 가져와 `effectiveAreaMult`에 곱해주어 최종 폭발/공격 반경을 넓게 설정합니다.

---

## 5. Emitter Strategy System (에미터 전략 패턴 / 모듈화)

방대해진 `DamageEmitter` 내부의 타격 및 스킬 연출 방식을 세분화하기 위해, **각 Emitter Type별로 고유한 클래스를 가지는 형태로 구조가 완전 분리**되었습니다.

- **1:1 매핑 메커니즘**: 플레이어가 획득한 각 무기(Skill ID)당 **정확히 1개의 고유한 `DamageEmitter` 인스턴스**가 생성되어 부착됩니다. 스킬 중복 적용 없이, 1개의 에미터가 해당 스킬의 발사 및 쿨다운을 전담합니다.
- `IEmitter` 인터페이스를 상속하여, `LinearEmitter`, `OrbitEmitter`, `ZoneEmitter`, `ArcEmitter`, `DirectionalEmitter`, `AuraEmitter`, `AoEEmitter`, `FieldStateEmitter` 등의 서브 클래스들이 개별 파일로 분리 구현되었습니다.
- `DamageEmitter`는 컨텍스트로 동작하며, `WeaponStats` 구조체를 통해 최적화된 사전 계산 스탯을(데미지, 투사체 수 등) 각 구체화된 Emitter 객체에 프로바이딩(`Update()` 인자 전달) 합니다.
- `EmitterFactory` 클래스가 템플릿의 `emitter_type` 문자열을 기반으로 적절한 서브 클래스 인스턴스를 동적으로 생성합니다.

---

## 5-1. 레벨업 (Level Up) 및 능력치 갱신 동작 원리

레벨업을 하더라도 `emitter_type`이나 부착된 `DamageEmitter` 객체가 파괴되고 재생성되지 않습니다.
1. 인벤토리에서 무기의 레벨이 증가하면, `Player::RefreshInventoryEffects`가 기존에 부착되어 있는 `DamageEmitter`의 `SetLevel(newLevel)`을 호출합니다.
2. `DamageEmitter`는 `WeaponData.json`에서 해당 `level`에 명시된 전용 추가 스탯(`damageMult`, `projectileCount`, `areaMult` 등)을 가져와 기본 데미지에 곱하거나 더합니다.
3. 이렇게 갱신된 수치는 즉시 `WeaponStats`을 통해 내부 `IEmitter` 서브 클래스로 전달되어 강력해진 공격이 그대로 실행됩니다.

---

## 6. 진화 시스템 (Weapon Evolution System)

무기가 최대 레벨(Max Level)에 도달하고 지정된 보조 패시브 아이템(`evolution_passive_id`)을 소지하고 있을 때 상위 무기로 자동 변환됩니다.
- **동작 방식**: 레벨업 옵션 반영 시점 등에서 `Player::CheckWeaponEvolutions`가 발동해 조건을 체크합니다. 조건 만족 시, 기존 무기와 Emitter는 파괴되고 `evolution_id`에 해당하는 신규 진화 무기로 대체됩니다.
- **예시**: `Whip` (Lv.8) + `Hollow Heart` -> `Bloody Whip`

---

## 7. 데이터 구조 (Json Schema Reference)

```json
{
    "id": "Number (Unique)",
    "name": "String",
    "emitter_type": "String (Class Name)",
    "damage": "Number",
    "tick_interval": "Number (Seconds)",
    "hit_radius": "Number (Main Hitbox)",
    "max_targets_per_tick": "Number",
    "traits": ["String Array"]
}
```
