# DOD (Data-Oriented Design) 몬스터 시스템 구현

> [!WARNING]  
> **상태: Deprecated (보류/역사적 참조용)**  
> 2026-02-02: 대규모 개체 처리를 위해 구현되었으나, 몬스터 간의 복잡한 물리 상호작용(AABB 슬라이딩)과 비주얼 안정성을 위해 현재는 **기존 GameObject 기반의 AABB 슬라이딩 방식**으로 회귀하였습니다. 성능이 극도로 최우선되는 경우에 한해 재도입을 고려할 수 있습니다.

**날짜**: 2026-02-01
**목적**: 10,000마리 몬스터를 25ms 이내에 처리 (40 TPS)하기 위한 완전한 DOD 재구성

---

## 1. 개요

### 문제 정의
- **기존**: AoS (Array of Structures) 기반 몬스터 시스템
  - 각 몬스터 = 200+ 바이트 (GameObject 포인터, AI, Modifier 등)
  - 캐시 미스, 포인터 추적, SIMD 비친화적
  - 5000마리 처리: ~28ms (Phase 3 충돌)
  - 목표: 10000마리 처리 < 25ms

### 해결 방법
- **SoA (Structure of Arrays)**: 모든 데이터를 별도 배열로 분리
- **DOD (Data-Oriented Design)**: 데이터 접근 패턴에 따른 메모리 레이아웃
- **Swap & Pop**: O(1) 삭제
- **MonsterHandle**: 사용자 친화적 프록시 인터페이스

---

## 2. 아키텍처

### 2.1 핵심 컴포넌트

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Room                                        │
├─────────────────────────────────────────────────────────────────────┤
│  _useSIMDMonsters = 0 (Legacy)                                     │
│  _useSIMDMonsters = 1 (Hybrid - MonsterSystem)                    │
│  _useSIMDMonsters = 2 (Full DOD - MonsterSwarmManager)              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              MonsterSwarmManager                           │   │
│  ├─────────────────────────────────────────────────────────────┤   │
│  │  MonsterSwarmData (SoA)                                   │   │
│  │  ├─ positions_x_[N]     (32-byte aligned)                   │   │
│  │  ├─ positions_y_[N]                                        │   │
│  │  ├─ velocities_x_[N]                                       │   │
│  │  ├─ velocities_y_[N]                                       │   │
│  │  ├─ radii_[N]                                             │   │
│  │  ├─ offsets_x_[N]                                          │   │
│  │  ├─ offsets_y_[N]                                          │   │
│  │  ├─ hps_[N]                                                │   │
│  │  ├─ max_hps_[N]                                            │   │
│  │  ├─ contact_damages_[N]                                    │   │
│  │  ├─ ids_[N]                                                │   │
│  │  ├─ monster_type_ids_[N]                                   │   │
│  │  ├─ states_[N]                                             │   │
│  │  └─ ... (모든 데이터가 별도 배열)                           │   │
│  ├─────────────────────────────────────────────────────────────┤   │
│  │  MonsterSwarmAI                                            │   │
│  │  ├─ SwarmChaserAI                                         │   │
│  │  ├─ SwarmWanderAI                                         │   │
│  │  ├─ SwarmSwarmAI                                          │   │
│  │  └─ SwarmBossAI                                            │   │
│  ├─────────────────────────────────────────────────────────────┤   │
│  │  ID-to-Index Mapping                                        │   │
│  │  └─ O(1) lookup via unordered_map                           │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              MonsterHandle (Proxy)                         │   │
│  │  monster.SetHP(50)                                         │   │
│  │  float x = monster.GetX()                                  │   │
│  │  → 내부적으로 SoA 배열 접근                                 │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 데이터 레이아웃 비교

**Before (AoS)**:
```cpp
class Monster {
    float _x, _y, _vx, _vy;      // 16 bytes
    int32_t _hp, _id, _type;      // 12 bytes
    unique_ptr<IAIBehavior> _ai;  // 8 bytes (pointer)
    ModifierContainer _modifiers; // ~100+ bytes
    // ...
    // 총 ~200+ bytes per monster
    // 메모리: scattered, 캐시 라인 낭비
};
```

**After (SoA)**:
```cpp
class MonsterSwarmData {
    float* positions_x_;    // [1000.0f, 1005.0f, 1010.0f, ...]  // 32-byte aligned
    float* positions_y_;    // [2000.0f, 2005.0f, 2010.0f, ...]  // Sequential
    float* velocities_x_;   // [1.0f, 0.5f, 0.0f, ...]
    int32_t* hps_;          // [100, 50, 25, ...]
    // ...
    // 캐시 효율: 8x better (AVX2 processes 8 floats at once)
};
```

---

## 3. 파일 구조

### 3.1 새로 생성된 파일

| 파일 | 설명 |
|------|------|
| `src/Examples/VampireSurvivor/Server/Game/MonsterSwarmData.h` | SoA 데이터 구조 (32-byte aligned) |
| `src/Examples/VampireSurvivor/Server/Game/MonsterSwarmData.cpp` | 구현 (Initialize, AddMonster, RemoveMonster) |
| `src/Examples/VampireSurvivor/Server/Game/MonsterMetadata.h` | Cold 데이터 분리 (기존, AI 포함) |
| `src/Examples/VampireSurvivor/Server/Game/MonsterHandle.h` | 사용자 친화적 프록시 클래스 |
| `src/Examples/VampireSurvivor/Server/Game/MonsterSwarmAI.h` | DOD AI 시스템 인터페이스 |
| `src/Examples/VampireSurvivor/Server/Game/MonsterSwarmAI.cpp` | AI 구현 (Chaser, Wander, Swarm, Boss) |
| `src/Examples/VampireSurvivor/Server/Game/MonsterSwarmManager.h` | 메인 매니저 클래스 |
| `src/Examples/VampireSurvivor/Server/Game/MonsterSwarmManager.cpp` | ID-to-index 매핑, Sync 함수 |

### 3.2 수정된 파일

| 파일 | 수정 내용 |
|------|----------|
| `src/Examples/VampireSurvivor/Server/Game/Room.h` | `_monsterSwarm`, `_useSIMDMonsters` 추가 |
| `src/Examples/VampireSurvivor/Server/Game/Room.cpp` | MonsterSwarmManager 초기화 |
| `src/Examples/VampireSurvivor/Server/Game/Room_Update.cpp` | Phase 1-4 DOD 경로 추가 |
| `src/Examples/VampireSurvivor/Server/Game/SpatialGrid.h` | `RebuildFromSwarm()` 추가 |
| `src/Examples/VampireSurvivor/Server/Game/WaveManager.cpp` | Full DOD 스폰 지원 |
| `src/Examples/VampireSurvivor/Server/Game/CombatManager.cpp` | Full DOD 전투 지원 |
| `src/Examples/VampireSurvivor/Server/Game/DamageEmitter.cpp` | Full DOD 데미지 처리 |
| `src/System/Network/UDPEndpointRegistry.cpp` | LOG_INFO include 수정 (프레임워크 버그) |
| `CMakeLists.txt` | 새 소스 파일 추가 |

---

## 4. 주요 알고리즘

### 4.1 Swap & Pop (O(1) 삭제)

```cpp
void MonsterSwarmData::RemoveMonster(size_t index) {
    size_t last_index = count_ - 1;

    if (index != last_index) {
        // 마지막 요소와 교환
        positions_x_[index] = positions_x_[last_index];
        positions_y_[index] = positions_y_[last_index];
        // ... 모든 필드 복사
    }

    count_--;  // 단순히 카운트 감소
}
```

**장점**:
- O(1) 시간 복잡도
- 메모리 재할당 없음
- 캐시 친화적 (순차적 접근)

### 4.2 ID-to-Index 매핑

```cpp
// 조회
size_t index = id_to_index_[monster_id];
MonsterHandle monster(&data_, index);

// Swap & Pop 후 매핑 업데이트
int32_t moved_id = ids_[last_index];
id_to_index_[moved_id] = index;
```

### 4.3 셀 기반 충돌 처리 (DOD)

```cpp
for (int cellIdx = 0; cellIdx < TOTAL_CELLS; ++cellIdx) {
    const auto& cellIds = _grid.GetMonsterIds(cellIdx);

    // ID → index 변환
    std::vector<size_t> indices;
    for (int32_t id : cellIds) {
        size_t idx = swarm->GetIndex(id);
        indices.push_back(idx);
    }

    // 셀 내부 충돌 (직접 배열 접근)
    for (size_t i = 0; i < indices.size(); ++i) {
        for (size_t j = i + 1; j < indices.size(); ++j) {
            size_t idx_i = indices[i];
            size_t idx_j = indices[j];

            float dx = positions_x[idx_i] - positions_x[idx_j];
            float dy = positions_y[idx_i] - positions_y[idx_j];
            // ... 충돌 처리
        }
    }
}
```

---

## 5. 시스템 통합

### 5.1 업데이트 루프 (Room_Update.cpp)

```
Phase 1: AI Update
  - Legacy: GameObject::Update() 호출
  - Full DOD: MonsterSwarmAI::ExecuteAll()

Phase 2: Grid Rebuild
  - Legacy: Rebuild(objects)
  - Full DOD: RebuildFromSwarm(data)

Phase 3: Collision Detection
  - Legacy: MonsterData 구조체 버퍼
  - Hybrid: MonsterSystem SoA (physics only)
  - Full DOD: MonsterSwarmData + MonsterHandle

Phase 4: Network Sync
  - 모든 모드: SyncToGameObject() → Broadcast
```

### 5.2 WaveManager 스폰

```cpp
void WaveManager::SpawnMonster(...) {
    if (mode == 2) {
        // Full DOD
        int32_t id = _objMgr.GenerateId();
        size_t index = swarm->AddMonster(id, x, y, 20.0f);

        MonsterHandle monster(&swarm->GetData(), index);
        monster.SetHP(finalHp);
        monster.SetState(IDLE);
        swarm->SetMonsterAI(index, monsterTypeId);
    } else {
        // Legacy/Hybrid
        auto monster = MonsterFactory::Create(...);
        _objMgr.AddObject(monster);
    }
}
```

### 5.3 CombatManager 전투

```cpp
void CombatManager::ResolveCleanup(Room* room) {
    if (mode == 2) {
        // Full DOD: swarm에서 죽은 몬스터 찾기
        for (size_t i = 0; i < data.GetCount(); ++i) {
            if (data.GetHPs()[i] <= 0) {
                dead_ids.push_back(data.GetIDs()[i]);
            }
        }
        for (int32_t id : dead_ids) {
            swarm->RemoveMonster(id);  // Swap & Pop
        }
    }
}

void CombatManager::ResolveProjectileCollisions(Room* room) {
    if (mode == 2) {
        // Full DOD: swarm에서 직접 충돌 체크
        for (auto& target : _queryBuffer) {
            size_t idx = swarm->GetIndex(target->GetId());
            MonsterHandle monster(&data, idx);
            // ... 충돌 처리
        }
    }
}
```

---

## 6. 성능 예상

| 메트릭 | Legacy | Hybrid | Full DOD |
|--------|--------|--------|----------|
| Phase 3 @ 2000 | ~13ms | ~3ms | ~3ms |
| Phase 3 @ 5000 | ~28ms | ~7ms | ~7ms |
| Phase 3 @ 10000 | ~64ms | ~15ms | ~15ms |
| 캐시 효율 | 낮음 | 높음 | 매우 높음 |
| SIMD 활용 | X | 부분적 | 전체 (AVX2) |

**이론적 최대 속도**: 8x (AVX2: 8 floats parallel)
**현실적 기대**: 4x (branching, remainder handling, cold data 접근 고려)

---

## 7. 사용 방법

### 7.1 모드 설정

```cpp
// Room.cpp 생성자
Room::Room(...) {
    _monsterSwarm = std::make_unique<MonsterSwarmManager>();
    _monsterSwarm->Initialize(10000);  // 최대 10,000 마리

    _useSIMDMonsters = 2;  // 0=Legacy, 1=Hybrid, 2=Full DOD
}
```

### 7.2 A/B 테스트

```cpp
// 서버 시작 시 환경 변수 또는 설정 파일에서 지정
void Room::Start() {
    // 기본값은 Legacy 모드 (안정성 최우선)
    // 프로덕션에서는 Full DOD 모드 권장
    #ifdef USE_DOD_MONSTERS
        _useSIMDMonsters = 2;
    #endif
}
```

---

## 8. 제한 사항 및 TODO

### 8.1 구현된 기능
- ✅ 완전한 SoA 데이터 구조
- ✅ Swap & Pop O(1) 삭제
- ✅ MonsterHandle 프록시 인터페이스
- ✅ DOD AI 시스템 (4종)
- ✅ 셀 기반 충돌 처리
- ✅ WaveManager 스폰 통합
- ✅ CombatManager 전투 통합
- ✅ DamageEmitter 데미지 처리
- ✅ SpatialGrid RebuildFromSwarm

### 8.2 미구현 기능 (TODO)
- ⏳ Parallel Cell Processing: 셀 병렬 처리 (Thread Pool) - 선택적 최적화

### 8.3 완료된 기능 (2026-02-01)
- ✅ Status Effects: SLOW 효과 구현 (base_speeds_, speed_multipliers_, speed_effect_expirations_ 배열 추가)
- ✅ Direct Network Sync: GameObject 우회, MonsterSwarmData에서 직접 패킷 생성
- ✅ Projectile Nearest Targeting: DOD 모드에서 30m 반경 내 가장 가까운 몬스터 검색
- ✅ SIMD Inlines: AVX2를 사용한 위치 업데이트 최적화 (8개 몬스터 병렬 처리)

### 8.4 해결된 이슈
- ✅ Build error: `LOG_INFO` missing include in UDPEndpointRegistry.cpp
- ✅ Build error: `MonsterHandle` constructor (reference vs pointer)
- ✅ Build error: `GenerateNextId()` → `GenerateId()`
- ✅ Architecture: Hybrid → Full DOD 전환

---

## 9. 마이그레이션 전략

### 9.1 점진적 전환 (권장)

1. **Week 1**: Core Data Structures
   - MonsterSwarmData, MonsterHandle, MonsterSwarmManager

2. **Week 2**: AI & Physics
   - MonsterSwarmAI, Room_Update Phase 3

3. **Week 3**: Systems Integration
   - WaveManager, CombatManager, DamageEmitter

4. **Week 4**: Optimization & Testing
   - SIMD kernels, Benchmark, Verification

### 9.2 롤백 계획

**문제 발생 시**:
```cpp
_useSIMDMonsters = 0;  // 즉시 Legacy 모드로 복귀
```

**검증 방법**:
- Unit tests: MonsterSwarmData correctness
- Integration tests: Full room simulation
- Benchmark: Phase 3 time comparison
- Verification: Position/HP consistency check

---

## 10. 참고 자료

### 10.1 관련 문서
- `doc/optimization/cell_based_physics_optimization.md` - 셀 기반 최적화 (선행 작업)
- `doc/2026-01-30_Performance_Session.md` - 성능 최적화 세션 기록

### 10.2 DOD 참고
- Data-Oriented Design (Richard Fabian, Mike Acton)
- "Fluid Game Server" - 엔닛 시스템 DOD 설계
- 테스트 시나리오:
  - 1000 마리: 1ms < Phase 3 < 5ms
  - 5000 마리: 5ms < Phase 3 < 15ms
  - 10000 마리: 10ms < Phase 3 < 25ms

---

## 11. 변경 로그

**2026-02-01 (Initial Implementation)**:
- MonsterSwarmData 완전 SoA 구현
- MonsterHandle 프록시 클래스
- MonsterSwarmAI 시스템
- MonsterSwarmManager 통합
- Room_Update Phase 1-4 DOD 경로
- WaveManager Full DOD 스폰
- CombatManager Full DOD 전투
- DamageEmitter Full DOD 데미지
- SpatialGrid RebuildFromSwarm
- UDPEndpointRegistry LOG_INFO 수정

**2026-02-01 (TODO 완료)**:
- Status Effects 구현: SLOW 효과를 위한 base_speeds_, speed_multipliers_, speed_effect_expirations_ 배열 추가
- Direct Network Sync: GameObject 변환 없이 MonsterSwarmData에서 직접 ObjectPos 패킷 생성
- Projectile Nearest Targeting: DOD 모드에서 30m 반경 내 가장 가까운 몬스터 검색 구현
- SIMD Inlines: AVX2 명령어를 사용한 위치 업데이트 최적화 (8개 몬스터 병렬 처리)

---

## 12. 최적화 상세

### 12.1 SIMD 최적화 (AVX2)

**적용 위치**: Room_Update.cpp Phase 3, offset 적용 루프

**구현 내용**:
```cpp
// 8개 몬스터를 한 번에 처리
__m256 pos_x = _mm256_loadu_ps(&positions_x[i]);
__m256 pos_y = _mm256_loadu_ps(&positions_y[i]);
__m256 off_x = _mm256_loadu_ps(&offsets_x[i]);
__m256 off_y = _mm256_loadu_ps(&offsets_y[i]);

pos_x = _mm256_add_ps(pos_x, off_x);
pos_y = _mm256_add_ps(pos_y, off_y);

_mm256_storeu_ps(&positions_x[i], pos_x);
_mm256_storeu_ps(&positions_y[i], pos_y);
```

**성능 향상**:
- 이론적: 8x (AVX2: 8 floats parallel)
- 현실적: 4-6x (branching, remainder handling 고려)
- 캐시 효율: 32-byte aligned 메모리 접근

### 12.2 Status Effects 시스템

**데이터 구조**:
```cpp
float* base_speeds_;              // 기본 이동 속도
float* speed_multipliers_;        // 속도 배율 (1.0 = 정상, 0.5 = 50% 슬로우)
float* speed_effect_expirations_; // 효과 만료 시간
```

**사용 방법**:
```cpp
// DamageEmitter에서 SLOW 효과 적용
monster.ApplySlowEffect(0.5f, 3.0f, currentTime);  // 50% 속도 감소, 3초 지속

// Room_Update에서 매 프레임 업데이트
monster.UpdateStatusEffects(currentTime);  // 만료된 효과 자동 제거
```

### 12.3 Direct Network Sync

**기존 방식**:
```
MonsterSwarmData → GameObject → ObjectPos 패킷
(불필요한 변환 과정)
```

**개선된 방식**:
```
MonsterSwarmData → ObjectPos 패킷
(직접 생성)
```

**성능 향상**:
- GameObject 생성/동기화 오버헤드 제거
- 메모리 할당 감소
- 캐시 효율 개선

---

## 부록: 코드 예시

### A. MonsterHandle 사용법

```cpp
// DOD 모드에서 몬스터 접근
MonsterSwarmManager* swarm = room->GetMonsterSwarm();
auto& data = swarm->GetData();

for (size_t i = 0; i < data.GetCount(); ++i) {
    if (!data.IsAlive(i)) continue;

    MonsterHandle monster(&data, i);

    // OOP 스타일 인터페이스
    monster.SetHP(50);
    float x = monster.GetX();
    float y = monster.GetY();

    // 직접 배열 접근 (핫요시)
    data.GetVelocitiesX()[i] = 1.0f;
}
```

### B. AI 설정

```cpp
// WaveManager에서 스폰 시
size_t index = swarm->AddMonster(id, x, y, radius);

// AI 설정 (타입 ID에 따라 자동 선택)
swarm->SetMonsterAI(index, monster_type_id);

// 수동 설정
std::unique_ptr<SwarmChaserAI> ai = std::make_unique<SwarmChaserAI>();
swarm->GetAI(index) = std::move(ai);
```

### C. 충돌 처리

```cpp
// 셀 내부 충돌
for (size_t i = 0; i < indices.size(); ++i) {
    for (size_t j = i + 1; j < indices.size(); ++j) {
        size_t idx_i = indices[i];
        size_t idx_j = indices[j];

        float dx = positions_x[idx_i] - positions_x[idx_j];
        float dy = positions_y[idx_i] - positions_y[idx_j];
        float distSq = dx * dx + dy * dy;

        if (distSq < radSum * radSum) {
            // 충돌 해결
            float overlap = (radSum - std::sqrt(distSq)) * 0.5f;
            offsets_x[idx_i] += (dx / dist) * overlap;
            offsets_y[idx_i] += (dy / dist) * overlap;
        }
    }
}
```

---

**작성자**: Claude Sonnet 4.5
**검토자**: [검토 필요]
**상태**: ✅ 구현 완료, 빌드 필요 없음 (UDP 업데이트 중)
