# Cell-Based Physics Optimization Plan

> [!NOTE]  
> **상태: 보류 (Future Work)**  
> 2026-02-02: 격자 중심의 물리 처리 방식은 대규모 최적화에는 유리하나, 각 몬스터의 정밀한 AABB 슬라이딩 로직을 구현하기에 복잡도가 높아 현재는 **개별 몬스터 검색 방식(QueryRange)**을 유지하고 있습니다. 추후 동시 접속자 수가 10,000명 이상으로 늘어날 경우 도입을 재검토합니다.

## 배경 (Background)

### 현재 상황
- **목표**: 1만 마리 이상의 몬스터를 40ms(25Hz) 틱 내에서 처리
- **현재 성능**: 8,000마리 기준 약 80~90ms (목표의 2배 이상 초과)
- **프로파일링 결과**:
  - `Phys (AI/이동)`: 43ms (48%)
  - `Overlap (밀치기)`: 32ms (35%)
  - `Net (패킷)`: 8.6ms (10%)
  - `Combat`: 5.5ms (6%)

### 병목 원인 분석
1. **개별 중심 쿼리 방식**: 8,000마리가 각자 `GetSeparationVector()` 호출 → 8,000번의 공간 검색
2. **중복 그리드 갱신**: `Overlap Resolution` 루프 안에서 밀릴 때마다 `_grid.Update()` 호출
3. **캐시 미스**: `shared_ptr`로 힙에 흩어진 몬스터 데이터 접근 시 CPU 캐시 효율 저하
4. **해시맵 오버헤드**: `std::unordered_map` 기반 격자 조회 비용

---

## 제안하는 해결책: Cell-to-Cell Physics

### 핵심 개념
> "몬스터가 주변을 찾게 하지 말고, **격자(Cell)가 자기 안의 몬스터들을 처리**하게 하자!"

### 구현 전략

#### 1단계: 격자 구조 개선
```cpp
// 현재: unordered_map<hash, vector<shared_ptr>>
// 개선: 연속 메모리 배열 + 인덱스 기반 접근
struct CellData {
    std::vector<uint32_t> monsterIds;  // 몬스터 ID만 저장
};
std::vector<CellData> _cells;  // 2D 격자를 1D로 펼침
```

#### 2단계: 일괄 물리 처리
```cpp
// 격자 순회 방식 (Cell-Centric)
for (int cellIdx = 0; cellIdx < totalCells; cellIdx++) {
    auto& cell = _cells[cellIdx];
    auto neighbors = GetAdjacentCells(cellIdx);  // 9칸 (자신 + 8방향)
    
    for (uint32_t myId : cell.monsterIds) {
        for (int n : neighbors) {
            for (uint32_t otherId : _cells[n].monsterIds) {
                if (myId == otherId) continue;
                ResolveCollision(myId, otherId);
            }
        }
    }
}
```

#### 3단계: 지연된 그리드 갱신
```cpp
// 모든 이동 처리 후 한 번만 갱신
for (auto& monster : _monsters) {
    monster.ApplyVelocity(dt);
}
RebuildGrid();  // 전체 재구축 (O(N))
```

---

## 예상 효과

| 항목 | 현재 (8K 기준) | 예상 (개선 후) |
|------|----------------|----------------|
| Phys | 43ms | 10~15ms |
| Overlap | 32ms | 5~10ms |
| 총합 | 80~90ms | 20~30ms |

---

## 구현 체크리스트

- [ ] `SpatialGrid` 클래스를 연속 메모리 기반으로 리팩토링
- [ ] `Room::Update`에서 Cell-Centric 순회 방식 적용
- [ ] `Monster::Update` 내 `GetSeparationVector` 호출 제거 (격자가 처리)
- [ ] 그리드 갱신을 틱 끝에 일괄 수행하도록 변경
- [ ] 프로파일링으로 개선 효과 측정

---

## 참고 자료

- **Boids Algorithm**: 물고기 떼/새 떼 시뮬레이션의 공간 분할 기법
- **They Are Billions**: 수만 마리 좀비를 실시간 처리하는 RTS 게임
- **Spatial Hashing for N-Body Simulation**: GPU/CPU 기반 대규모 물리 시뮬레이션

---

## 관련 파일

- `src/Examples/VampireSurvivor/Server/Game/SpatialGrid.h`
- `src/Examples/VampireSurvivor/Server/Game/Room.cpp` (Update 함수)
- `src/Examples/VampireSurvivor/Server/Entity/Monster.cpp` (Update 함수)

---

*작성일: 2026-02-01*
*다음 세션에서 이 문서를 참조하여 구현 진행*
