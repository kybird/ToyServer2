# Project TODO List

## 🚀 Tasks In-Progress & Remaining

### Server-side (Logic & Network)
- [ ] **[Network]** Room::SyncNetwork() 실제 로직 구현 (현재 플레이스홀더)
- [ ] **[Gameplay]** 유저 발사체 무기 유효거리(MaxRange) 체크 및 소멸 로직 추가
- [ ] **[Gameplay]** 유저 발사체 관통 시 데미지 감쇠 로직 구현 (매 관통당 10% 감소)
- [ ] **[Gameplay]** 크리티컬 히트 판정 로직 추가 (랜덤 확률 기반 데미지 배율 적용)
- [ ] **[Random]** 고성능 난수 생성기(Xorshift 또는 PCG 등) 도입 (표준 rand 대체로 성능 최적화)
- [ ] **[Spawning]** 몬스터 스폰 밀집도 분산 로직 (SpatialGrid를 활용하여 플레이어 주변 밀집도가 낮은 영역에 우선 생성)
- [ ] **[Gameplay]** 신규 무기 '대검' 구현 (휘두르기 범위 데미지 판정 및 업그레이드 시 반경 증가 적용)

### Client-side (UI & Visuals)
- [ ] **[UI]** 몬스터 HP 바 노출 정책 변경 (평소엔 숨김, 피격 시에만 일정 시간 표시)
- [ ] **[FX]** 데미지 숫자 연출 전면 개선 (폰트 크기 확대, 시인성 확보, 생성 시 Scaling 임팩트 추가)
- [ ] **[FX]** 크리티컬 히트 발생 시 데미지 숫자 강조 (색상 변경 및 크기 확대 연출)
- [ ] **[UI]** 보유 중인 무기/패시브 리스트 및 각 레벨을 표시하는 인게임 HUD 추가
- [ ] **[FX]** '대검' 무기의 휘두르기(Swipe) 궤적 이펙트 구현

### Infrastructure & Refactoring
- [ ] **[Clean]** 전역적으로 사용되지 않는 레거시 코드 및 주석 제거 (진행 중)
- [ ] **[Test]** Integration test with VampireSurvivor logic (UDP/KCP)
- [ ] **[Perf]** Performance profiling of UDP pooling and KCP overhead
- [ ] **[Docs]** Documentation of the revised networking architecture

### Shared & Integration
- [ ] **[Protocol]** 서버-클라이언트 간 새로운 연출 정보(크리티컬 발동 등) 동기화 필드 추가

---

## ✅ Completed Tasks

### Infrastructure & Network (Recent)
- [x] **[UDP]** UDP 세션 풀링(Pool) 도입 및 `SessionFactory` 연동 (메모리 파편화 및 할당 비용 최적화)
- [x] **[UDP]** Reliable/Unreliable Routing (KCP integration 완료)
- [x] **[KCP]** Integrate KCP for Reliable UDP (Action packets)
- [x] **[UDP]** Use Raw UDP for Unreliable packets (Movement packets)
- [x] **[Session]** Implement standard `OnRecycle` cleanup for sessions
- [x] **[Decision]** Session Lifecycle Refactoring: Keep SessionFactory exclusivity for safety

### Gameplay & Systems
- [x] **[Stat]** 패시브 아이템 효과 실제 플레이어 스탯 실시간 반영 (글로벌 패시브 시스템 도입)
- [x] **[Gameplay]** Frost Nova(AOE) 업그레이드 로직 수정 (특성 기반 시스템 적용 완료)
- [x] **[Refactor]** 뱀파이어 서바이벌 방식의 특성 기반(Trait-based) 글로벌 패시브 시스템으로 전면 개편

### UI & Visuals
- [x] **[Visual]** 유니티 배경 격자무늬(Grid) 표시 기능 (웹 비주얼라이저 스타일 구현)
- [x] **[Visualizer]** 웹 비주얼라이저 특정 유저 추적(Follow) 및 뷰포트 선택 기능 추가

### Documentation
- [x] **[Docs]** 무기 및 패시브 시스템 상세 정리 문서(`doc/Skills.md`)
- [x] **[Docs]** UDP 패킷 분류 및 Reliable UDP 전략 문서(`doc/udp_packet_classification.md`)
- [x] **[Docs]** SessionFactory 설계 문제 분석 문서(`doc/sessionfactory_design_issues.md`)

---
*마지막 업데이트: 2026-02-07 (UDP KCP 통합 및 TODO 구조 정리)*
