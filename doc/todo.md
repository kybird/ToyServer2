# Project TODO List

## 🚀 Tasks In-Progress & Remaining

### Client-side (UI & Visuals)
- [ ] **[UI]** 몬스터 HP 바 노출 정책 변경 (평소엔 숨김, 피격 시에만 일정 시간 표시)
- [ ] **[FX]** 데미지 숫자 연출 전면 개선 (폰트 크기 확대, 시인성 확보, 생성 시 Scaling 임팩트 추가)
- [ ] **[FX]** 크리티컬 히트 발생 시 데미지 숫자 강조 (색상 변경 및 크기 확대 연출)
- [ ] **[UI]** 보유 중인 무기/패시브 리스트 및 각 레벨을 표시하는 인게임 HUD 추가
- [ ] **[FX]** '대검' 무기의 휘두르기(Swipe) 궤적 이펙트 구현

### Infrastructure & Refactoring
- [ ] **[Clean]** 전역적으로 사용되지 않는 레거시 코드 및 주석 제거 (진행 중)
- [ ] **[Perf]** Performance profiling of UDP pooling and KCP overhead
- [ ] **[Docs]** Documentation of the revised networking architecture (상세화 작업 필요)

---

## ✅ Completed Tasks

### Server-side (Logic & Network)
- [x] **[Network]** `Room::SyncNetwork()` 구현 (`S_MoveObjectBatch` 및 `S_PlayerStateAck` 동기화 로직 완료)
- [x] **[Gameplay]** 유저 발사체 무기 유효거리(MaxRange) 체크 및 소멸 로직 (`Projectile.h:80`)
- [x] **[Gameplay]** 유저 발사체 관통 데미지 감쇠 로직 적용 (매 관통당 10% 감소, `CombatManager.cpp:153`)
- [x] **[Gameplay]** 크리티컬 히트 시스템 구현 (랜덤 확률 기반 데미지 배율 적용 완료)
- [x] **[Random]** 고성능 난수 생성기 `System::Utility::FastRandom` 도입 및 전역 적용
- [x] **[Spawning]** `AngularGap` 기반 몬스터 스폰 분산 로직 구현 (`WaveManager.cpp:206`)
- [x] **[Gameplay]** 신규 무기 '대검' 구현 (`Arc` 이미터 및 부채꼴 범위 판정 로직 완료)
- [x] **[Test]** 테스트 빌드 에러 수정 (MockSession/TrackingMockSession의 `SendReliable`, `SendUnreliable` 구현 완료)

### Infrastructure & Network
- [x] **[UDP]** UDP 세션 풀링(Pool) 도입 및 `SessionFactory` 연동
- [x] **[UDP]** Reliable/Unreliable Routing (KCP integration 완료)
- [x] **[KCP]** Integrate KCP for Reliable UDP (Action packets)
- [x] **[UDP]** Use Raw UDP for Unreliable packets (Movement packets)
- [x] **[Session]** Implement standard `OnRecycle` cleanup for sessions
- [x] **[Decision]** Session Lifecycle Refactoring: Keep SessionFactory exclusivity for safety

### Gameplay & Systems
- [x] **[Stat]** 패시브 아이템 효과 실제 플레이어 스탯 실시간 반영
- [x] **[Gameplay]** Frost Nova(AOE) 업그레이드 로직 수정
- [x] **[Refactor]** 뱀파이어 서바이벌 방식의 특성 기반(Trait-based) 글로벌 패시브 시스템 개편

### UI & Visuals (Protocol)
- [x] **[Protocol]** 서버-클라이언트 간 새로운 연출 정보(크리티컬, 대검 각도 등) 동기화 필드 추가 (`game.proto`)
- [x] **[Visual]** 유니티 배경 격자무늬(Grid) 표시 기능
- [x] **[Visualizer]** 웹 비주얼라이저 특정 유저 추적 및 뷰포트 선택 기능

### Documentation
- [x] **[Docs]** 무기 및 패시브 시스템 상세 정리 문서(`doc/Skills.md`)
- [x] **[Docs]** UDP 패킷 분류 및 Reliable UDP 전략 문서(`doc/udp_packet_classification.md`)
- [x] **[Docs]** SessionFactory 설계 문제 분석 문서(`doc/sessionfactory_design_issues.md`)

---
*마지막 업데이트: 2026-02-07 (서버 로직 전수 조사 및 실환경 동기화)*
