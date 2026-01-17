# 구현 계획서: 뱀서류 게임 완성 로드맵

> 📅 작성일: 2026-01-15
> 📊 기반: doc/todo.md 항목 분석

---

## 📋 TODO 분석 및 우선순위

### 🔴 P1: 긴급 버그 수정 (Critical Fixes)

| 항목 | TODO | 현재 상태 | 해결 방안 | 예상 시간 |
|------|------|-----------|-----------|-----------|
| **#5** | 피격시 빨간색 변한 뒤 안돌아오는 문제 | 🐛 버그 | 클라이언트 Material 상태 리셋 로직 추가 | 0.5일 |
| **#15** | 플레이어 발사체 무한 비행 | 🐛 버그 | 투사체에 lifetime 또는 max_distance 추가 | 0.5일 |

### 🟠 P2: 게임플레이 핵심 (Core Gameplay)

| 항목 | TODO | 구현 내용 | 예상 시간 |
|------|------|-----------|-----------|
| **#1** | 레벨업 스킬 선택 UI | Unity UI 구현 (3개 선택지 카드, 시간 정지 연출) | 1-2일 |
| **#2** | 레벨업 중 의문사 방지 | 서버: 레벨업 중 무적 상태 | 0.5-1일 |
| **#3** | 웨이브 시작 알림 UI | 화면 중앙 텍스트 표시 (이름, 지속시간) | 0.5일 |
| **#14** | 몬스터 죽음시 경험치 드랍 | ExpGem 엔티티 생성, 자석 효과, 병합 로직 | 2-3일 |

### 🟡 P3: 콘텐츠 확장 (Content Expansion)

| 항목 | TODO | 구현 내용 | 예상 시간 |
|------|------|-----------|-----------|
| **#4** | 스플래시/관통 데미지 스킬 | DamageEmitter 확장 (AoE, Piercing 타입) | 1-2일 |
| **#6** | 유도탄 공격 추가 | Projectile에 호밍 로직 추가 | 1-2일 |
| **#7** | 장판 공격 추가 | 새로운 Effect 타입 (지속 데미지 존) | 1-2일 |

### 🟢 P4: 최적화 및 품질 개선 (Optimization & Polish)

| 항목 | TODO | 구현 내용 | 예상 시간 |
|------|------|-----------|-----------|
| **#9** | 몬스터 패킷 전송 빈도 최적화 | 25fps → 3fps로 다운샘플링, 델타 압축 | 1-2일 |
| **#11** | 몬스터 HP바 제거 | UI 렌더링 부하 감소 | 0.2일 |
| **#12** | 몬스터 피격 효과 추가 | Hit Flash, Knockback 시각화 | 0.5일 |
| **#13** | 웨이브시 몬스터 HP 증가 확인 | WaveData.json hp_multiplier 검증 | 0.2일 |

### 🔵 P5: 멀티플레이 검증 (Multiplayer Testing)

| 항목 | TODO | 구현 내용 | 예상 시간 |
|------|------|-----------|-----------|
| **#8** | 멀티플레이 테스트 | 2-4인 동시 플레이 스트레스 테스트 | 1일 |
| **#10** | 서버 기능 확장 아이디어 | 리더보드, 채팅, 통계 시스템 등 | TBD |

---

## 🚀 4주 로드맵

```
Week 1: 핵심 UX 완성
├─ Day 1-2: [#1] 레벨업 UI 구현
├─ Day 3: [#2] 레벨업 무적 처리
├─ Day 4: [#5, #15] 버그 수정
└─ Day 5: [#3] 웨이브 알림 UI

Week 2: 경험치 시스템
├─ Day 1-3: [#14] ExpGem 드랍 및 수집
├─ Day 4: [#13] 웨이브 HP 스케일링 검증
└─ Day 5: [#8] 멀티플레이 테스트

Week 3: 스킬 다양화
├─ Day 1-2: [#4] 스플래시/관통 스킬
├─ Day 3-4: [#6] 유도탄 시스템
└─ Day 5: [#7] 장판 공격

Week 4: 최적화 및 폴리싱
├─ Day 1-2: [#9] 패킷 전송 최적화
├─ Day 3: [#12] 몬스터 피격 효과
├─ Day 4: [#11] HP바 최적화
└─ Day 5: 종합 테스트 및 밸런싱
```

---

## 📁 신규/수정 파일 목록

### Server (C++)
```
src/Examples/VampireSurvivor/
├── Server/Game/
│   ├── CombatManager.cpp (수정: 무적 체크, 스플래시/관통/장판)
│   ├── DamageEmitter.cpp (수정: 유도탄, AoE)
│   ├── WaveManager.cpp (수정: UI 알림 패킷)
│   └── ExpGemManager.h/cpp (신규: 젬 생성/병합)
├── Entity/
│   ├── ExpGem.h/cpp (신규)
│   ├── Projectile.cpp (수정: 호밍, lifetime)
│   └── Player.cpp (수정: 레벨업 무적)
└── Protocol/
    └── game.proto (확장: timeout, S_WaveNotify)
```

### Client (Unity C#)
```
Assets/_Project/Scripts/
├── UI/
│   ├── LevelUpUI.cs (신규: 3-option 선택 UI)
│   ├── LevelUpOptionCard.cs (신규: 옵션 카드)
│   ├── WaveNotificationUI.cs (신규: 웨이브 알림)
│   └── InGameUI.cs (수정: ExpBar 추가)
├── Game/
│   ├── ExpGemController.cs (신규: 젬 흡수)
│   └── MaterialFlasher.cs (신규: 피격 효과 리셋)
└── Network/
    └── PacketHandler.cs (수정: 핸들러 추가)
```

---

## 🔧 기술적 상세

### 1. 레벨업 무적 시스템 (Real-time Sync 방식)

**서버 (Player.h)**
- `LEVELING_UP` 상태일 때 `TakeDamage`와 `ApplyInput`을 차단하여 무적 및 이동 불가 처리.
- 클라이언트는 여전히 다른 객체들의 보간(Interpolation)을 실시간으로 수행함.

```cpp
enum class PlayerState : uint8_t {
    NORMAL = 0,
    LEVELING_UP = 1,
    DOWNED = 2,
    DEAD = 3
};

void Player::TakeDamage(int32_t damage, Room *room) {
    if (_isLevelingUp) return; // 레벨업 중 무적
    // ...
}
```

**클라이언트 (LevelUpUI.cs)**
- `TickManager`가 `realtimeSinceStartup` 기반으로 동작하므로 `timeScale = 0`을 사용하지 않음.
- UI는 `unscaledDeltaTime`을 사용하여 실시간 연출 및 타이머 처리.

```csharp
public void Show(List<LevelUpOption> options, float timeout) {
    _isActive = true;
    _currentTimeout = timeout;
    _panelContainer.SetActive(true);
    // 애니메이션 및 타이머 시작 (unscaledDeltaTime 기반)
}
```

### 2. 패킷 최적화 (#9)

**현재**: 25fps 전송 (40ms 간격)
**목표**: 3fps 전송 (333ms 간격)

```cpp
// 옵션 A: 다운샘플링
if (_updateCounter % 8 == 0) {  // 25/8 ≈ 3fps
    BroadcastMonsterPositions();
}

// 옵션 B: Dirty Flag
if (monster->HasMovedSignificantly(threshold = 0.5f)) {
    QueueForBroadcast(monster);
}
```

### 3. ExpGem 병합

```cpp
void ExpGemManager::MergeNearbyGems() {
    for (auto& gem : _gems) {
        auto nearby = QueryNearby(gem->pos, MERGE_RADIUS);
        if (nearby.size() >= 3) {
            CreateMergedGem(gem->pos, TotalExp(nearby));
            RemoveGems(nearby);
        }
    }
}
```

---

## ✅ Week 1 Day 1-2 상세 체크리스트

### 서버 (C++)
- [x] Player::EnterLevelUpState() 구현
- [x] Player::ExitLevelUpState() 구현
- [x] Player::IsLevelingUp() getter 추가
- [x] CombatManager: 레벨업 중 무적 체크 (Player::TakeDamage에서 처리)
- [x] S_LevelUpOption에 timeout_seconds 필드 추가
- [x] LevelUpManager: timeout 값 패킷에 포함 (Player.cpp에서 30.0f로 하드코딩됨)

### 클라이언트 (Unity)
- [x] LevelUpUI.cs 작성
- [x] LevelUpOptionCard.cs 작성
- [x] PacketHandler: Handle_S_LevelUpOption 확장
- [x] Time.timeScale 복원 안전성 검증 (Real-time Sync 방식으로 변경되어 timeScale 제어가 불필요해짐)
- [x] UI Prefab 구조 설계 (코드상 SerializeField 구성 확인)

### 프로토콜 (Protobuf)
- [x] game.proto에 timeout_seconds 필드 추가
- [x] 프로토 재생성 (서버에서 필드 사용 확인)

---

## 📝 참고 문서

- [GameCompletionPlan.md](./GameCompletionPlan.md) - 전체 로드맵
- [2026-01-14_DevLog.md](./2026-01-14_DevLog.md) - 레벨업 시스템 기본 구현
- [coding_convention.md](./coding_convention.md) - 코딩 컨벤션
