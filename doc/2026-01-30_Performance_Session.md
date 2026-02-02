# 2026-01-30 Performance Tuning Session - Monster Swarm Optimization

## 1. 현재 상태 (Diagnostic)
- **대상**: 10,000마리 규모의 몬스터 시뮬레이션
- **현상**:
    - 몬스터 수가 늘어남에 따라 `AvgUpdate` 가 선형적으로 증가 (7,500마리 기준 약 88ms 소요, 틱 한계 50ms 초과).
    - "과거에서 사는 느낌" (Client-Side Lag): 서버 시뮬레이션 지연이 패킷 전송 지연으로 이어짐.
    - 배칭(Batching) 및 디더링(Dithering) 도입 후 비주얼 저하 (몬스터 겹침 현상 발생).

## 2. 수행한 작업 (Optimizations Applied)
- **Room::Update 구조화**: 로직(Combat) -> 물리(Physics) -> 통신(Sync) 순서로 명확히 분리하여 로직 반응성 확보.
- **Shared_ptr 복사 방지**: `GetAllObjects()` 시 발생하는 대량의 Atomic RefCount 연산을 `const reference` 전달 방식으로 변경하여 CPU 오버헤드 완화.
- **분산 동기화 (Distributed Sync)**: 10,000마리의 데이터를 5틱마다 한 번에 보내던 Burst 방식을 매 틱당 20%씩 나누어 보내도록 깎음 (Network Smoothing).
- **물리 연산 디더링 (Monster Dithering)**: `Monster::Update` 에서 수행하는 `GetSeparationVector` (Grid Query)를 4틱당 1회로 제한. (※ 이로 인해 비주얼 이슈 발생 확인)

## 3. 향후 핵심 과제 (Next Actions)
- [ ] **Grid-Base Collision**: 모든 몬스터가 주변을 검색($O(N \cdot K)$)하는 대신, 그리드 셀이 포함된 몬스터끼리 연산하도록 구조 변경.
- [ ] **Type Casting 제거**: 핫패스 내 `dynamic_pointer_cast`를 제거하고 `enum` 기반 타입 체크로 전환.
- [ ] **Visual Recovery**: 디더링 주기를 줄이거나, 그리드 기반 연산 최적화를 통해 매 프레임 충돌 판정이 가능하도록 복구.
- [ ] **Gateway Linearization 검토**: `GatewaySession`의 `memcpy` 및 암호화 병목이 전송 지연에 미치는 영향 재분석.

## 4. 임시 메모리 (Session Notes)
- 1,800마리에서 69ms가 나오는 것은 단순한 코드 분산으로는 '범용 프레임워크'의 오버헤드를 극복하지 못함을 시사함.
- 알고리즘 수준의 최적화(Spatial Partitioning 활용 극대화)가 필요함.
