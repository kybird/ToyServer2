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

- **PROJECTILE**: 투사체 개수 증가(`Duplicator`), 관통력 증가(`Magic Bindi`) 적용 대상.
- **AREA**: 공격 범위 및 폭발 반경 증가(`Candelabrador`) 적용 대상.
- **DURATION**: 지속 시간 증가(`Spellbinder`) 적용 대상.
- **MELEE**: 근접 공격 관련 보너스 적용 대상.
- **EFFECT**: 상태 이상(독, 슬로우 등) 효과 관련 보너스 적용 대상.

---

## 5. 데이터 구조 (Json Schema Reference)

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
