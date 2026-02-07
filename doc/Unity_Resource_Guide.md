# 유니티 리소스 연결 가이드 (Unity Resource Mapping Guide)

이 문서는 `ObjectManager.cs` 및 `InventoryHUD.cs` 등 클라이언트 구현 코드에서 참조하는 리소스 경로와 네이밍 규칙을 설명합니다. 리소스가 없을 경우 시스템에서 기본 Placeholder를 사용하지만, 정밀한 연출을 위해 아래 규칙에 맞춰 프리팹과 텍스처를 배치해 주세요.

---

## 1. 프리팹 (Prefabs)
모든 프리팹은 `Assets/_Project/Resources/Prefabs/` 경로에 위치해야 합니다.

| 리소스 이름 | 설명 | 비고 |
| :--- | :--- | :--- |
| `Player` | 플레이어 캐릭터 프리팹 | `ObjectType.Player` 기본값 |
| `Monster_{ID}` | 몬스터 타입별 프리팹 | 예: `Monster_1` (슬라임), `Monster_2` (박쥐) |
| `Projectile_{ID}` | 투사체 타입별 프리팹 | 예: `Projectile_1` (단검) |
| `Item` | 필드 드랍 아이템(경험치 젬 등) | `ExpGemController.cs` 부착 권장 |
| `DamageText` | 데미지 표시 텍스트 프리팹 | `TextMeshPro` 및 `DamageText.cs` 부착 필요 |
| `SkillEffect_{ID}` | 스킬 연출 프리팹 | 스케일은 서버 `radius`에 따라 자동 조절됨 |

> **Tip**: `SkillEffect_3`(Frost Nova), `SkillEffect_4`(Greatsword)는 코드 기반으로 자동 생성되므로 프리팹이 없어도 정상 작동합니다.

---

## 2. 인벤토리 아이콘 (Textures)
모든 아이콘은 `Assets/_Project/Resources/Textures/` 경로에 `.png` 또는 `.sprite` 리소스로 위치해야 합니다.

| 파일 이름 | 대상 아이템 (InventoryHUD) | 비고 |
| :--- | :--- | :--- |
| `dagger` | 단검 (Weapon ID: 1, 4) | Greatsword 포함 |
| `dart` | 독침 (Weapon ID: 2) | |
| `MagicBolt` | 마법 화살 (Weapon ID: 3) | 기본 Fallback 아이콘 |
| `Icon_Exp` | 경험치 증가 패시브 (Passive ID: 1) | Spinach 역할 |
| `Icon_Heart` | 최대 체력 패시브 (Passive ID: 2) | Hollow Heart 역할 |
| `Icon_Level` | 기타 레벨업 장식 | 패시브 Fallback |

---

## 3. 사운드 (Sound)
`SoundManager.cs`가 존재하는 경우 아래 이름의 오디오 클립을 참조합니다.

- `Hit`: 타격 시 재생 (기본값)
- `Skill_{ID}`: 스킬 시전 시 재생 (예: `Skill_3`)
- `LevelUp`: 플레이어 레벨업 및 옵션창 등장 시 재생

---

## 4. 구현 특이사항 및 주의사항
1. **스케일 자동 조절**: `ObjectManager`에서 스킬 이펙트 생성 시 서버의 `radius` 값을 기반으로 `localScale`을 강제 조정합니다. 따라서 프리팹 제작 시 **기본 크기(Radius 1.0)를 1x1x1** 사이즈로 맞춰주시는 것이 가장 정확합니다.
2. **빌보드**: HP바(`HpBar.cs`)는 `LateUpdate`에서 카메라 방향과 상관없이 정면을 보도록 되어 있습니다. (`Quaternion.identity`)
3. **Critical 연출**: 크리티컬 발생 시 `SimpleFlash` 컴포넌트가 적을 노란색(Yellow)으로 반짝이게 하며, `DamageText`는 더 크게 팝업됩니다.

---
*최종 업데이트: 2026-02-07 (구현 코드 기반 작성)*
