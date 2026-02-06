# 🎮 Vampire Survivor Toy Project Resource Guide

이 문서는 개발된 시스템이 정상적으로 리소스를 불러오기 위해 필요한 파일 경로와 이름 규칙을 설명합니다.

---

## 1. 🖼️ UI 및 아이콘 (Sprites)
모든 이미지는 **Texture Type**이 `Sprite (2D and UI)`로 설정되어야 하며, 아래 경로에 위치해야 합니다.
- **경로**: `Assets/_Project/Resources/Textures/`

### 필수 아이콘 목록
| 시스템 | 파일명 | 설명 |
| :--- | :--- | :--- |
| **무기** | `dagger.png` | ID 1 (단검), ID 4 (대검) 공유 |
| | `dart.png` | ID 2 (독침) |
| | `MagicBolt.png` | ID 3 (Frost Nova) 및 기타 발사체 |
| **패시브**| `Icon_Exp.png` | ID 1 (Spinach - 공격력) |
| | `Icon_Heart.png` | ID 2 (Hollow Heart - 체력) |
| | `Icon_Level.png` | ID 3~7, 11 (Wings, Tome 등 공용) |

---

## 2. 🔊 사운드 (AudioClips)
사운드 파일은 `SoundManager`가 자동으로 로드하며, MP3, WAV, OGG 형식을 지원합니다.
- **경로**: `Assets/_Project/Resources/Sounds/`

### 필수 사운드 목록
| 이벤트 | 파일명 | 설명 |
| :--- | :--- | :--- |
| **피격** | `Hit.xxx` | 몬스터가 데미지를 입었을 때 |
| **레벨업** | `LevelUp.xxx` | 플레이어가 레벨업 했을 때 |
| **스킬** | `Skill_{ID}.xxx` | 특정 스킬 사용 시 (ex: `Skill_1.wav`) |

---

## 3. 📦 프리팹 (Prefabs)
특수 효과 및 동적 생성 오브젝트를 위한 설정입니다.
- **경로**: `Assets/_Project/Resources/Prefabs/`

### 필수 프리팹 목록
- `DamageText.prefab`: 데미지 숫자를 띄우는 UI (World Space Canvas 권장)
- `SkillEffect_{ID}.prefab`: 스킬 발동 시 해당 위치에 생성될 이펙트 (ex: `SkillEffect_1.prefab`)
- `Monster_{ID}.prefab`: 서버의 몬스터 ID와 매칭되는 프리팹 (ex: `Monster_1.prefab`)

---

## 4. 🛠️ UI 셋업 주의사항 (Unity Inspector)
`InventoryHUD` 스크립트가 붙은 오브젝트에는 다음 연결이 필요합니다.
1. **Weapon Container**: 무기 아이콘이 담길 RectTransform (Horizontal/Grid Layout Group 권장)
2. **Passive Container**: 패시브 아이콘이 담길 RectTransform
3. **Icon Prefab**: 아이콘 하나를 표시할 기본 Image 프리팹 (내부에 `TextMeshPro - Text`가 있으면 레벨 정수 표시 가능)

---

> **Tip**: 리소스가 없어도 시스템은 에러 없이 동작하며, 로그(`Debug.LogWarning`)를 통해 어떤 파일이 누락되었는지 리포트합니다.
