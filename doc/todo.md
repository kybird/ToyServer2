# 할 일 (Todo) - 우선순위 정렬

> 📅 마지막 업데이트: 2025-12-20

---

## 🔴 P1: 아키텍처 위반 해결

### PacketMessage 직접 접근 캡슐화
- **상태**: 검토 중
- **파일**: `GamePacketHandler.h/cpp`, `Room.h/cpp`
- **설명**: `PacketMessage` 내부 멤버(`Payload()`, `type`) 직접 접근 제거
- **해결방안**: 인터페이스 기반 데이터 접근 or `PacketUtils` 루트 노출
- **난이도**: ⭐⭐ (반나절)
- **이유**: 프레임워크 설계 원칙 준수

---

## 🟡 P2: 게임 로직 - 엔진 검증 (spec.md P1)

### 대량 몬스터 스폰 & 이동 동기화
- **상태**: 미완료
- **목표**: 한 화면에 500+ 마리 몬스터 스폰 및 위치 동기화
- **구현사항**:
  - [ ] `MonsterFactory` 대량 생성 최적화
  - [ ] `S_MoveObjectBatch` 패킷 구현 (Batching)
  - [ ] Delta Compression 적용
  - [ ] Grid 기반 Spatial Partitioning
- **난이도**: ⭐⭐⭐ (2~3일)

---

## 🟢 P3: 게임 로직 - 게임 루프 (spec.md P2)

### 투사체 시스템
- [ ] `S_SpawnProjectile` 구현
- [ ] 서버 권위적 충돌 판정

### 웨이브 시스템
- [ ] `WaveDataTable` JSON 로드
- [ ] 시간 기반 스폰 패턴 구현
- [ ] 유저 수 기반 난이도 스케일링

### 게임 승/패 조건
- [ ] 5분 생존 시 `S_GameWin`
- [ ] 전원 DOWNED 시 `S_GameOver`
- [ ] 결과 화면 및 보상 처리

---

## 🔵 P4: 콘텐츠 확장 (spec.md P3)

### 클래스 시스템
- [ ] Knight (Tanker) - Taunt 스킬
- [ ] Mage (Nuker) - Meteor 스킬
- [ ] Priest (Healer) - Sanctuary 스킬
- [ ] `C_SelectClass` / `S_ClassChange` 패킷

### 부활 시스템
- [ ] DOWNED 상태 (묘비 표시)
- [ ] 동료 근접 3초 유지 시 부활
- [ ] `S_PlayerDowned`, `S_ReviveProgress`, `S_PlayerRevive`

### 스킬 진화 시스템
- [ ] Weapon Max + Passive + Box = Evolution
- [ ] `EvolutionData.json` 로드
- [ ] `S_EvolutionEffect` 패킷

---

## ⚪ P5: 완성도 (spec.md P4)

### 채팅 시스템
- [ ] 월드 채팅 (Lobby)
- [ ] 룸 채팅 (In-Game)
- [ ] `ChatManager` 구현

### 아웃게임 성장 (Meta-Progression)
- [ ] 스킬 트리 UI 데이터 (`S_SkillTreeInfo`)
- [ ] 노드 업그레이드 (`C_UpgradeSkillNode`)
- [ ] DB 트랜잭션 (포인트 차감)

---

## 💤 연기됨 (Deferred)

### File Watcher
- **설명**: Config/Script 핫 리로딩
- **이유**: 핵심 게임 로직에 우선순위 부여

### Metrics Dashboard
- **설명**: 서버 모니터링 대시보드
- **이유**: 운영 단계에서 필요

### SessionFactory DI 패턴 리팩토링
- **설명**: Static → Instance 기반 변경
- **이유**: 현재 동작에는 문제 없음

### 데이터베이스 모니터링
- **설명**: `IDatabaseMonitor` 인터페이스
- **이유**: 매우 낮은 우선순위
