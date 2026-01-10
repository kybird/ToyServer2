# 할 일 (Todo) - 우선순위 정렬

> 📅 마지막 업데이트: 2025-12-20

---



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

## 🟠 P2.5: 프레임워크 안전성 개선 (MCP 분석 결과)

### RoomManager 동시성 개선
- **상태**: 제안됨
- **설명**: `RoomManager`의 Global Mutex 의존 제거 및 세분화 (Lobby vs Room Lock 분리)
- **이유**: 동접 증가 시 병목 예상

### Session 생명주기 안전장치
- **상태**: 제안됨
- **설명**: `OnSessionDisconnect` 수동 호출 의존성 제거, `WeakPtr` 또는 `Handle` 기반 자동 만료 도입
- **이유**: 유지보수 실수로 인한 Dangling Pointer 방지

### RoomManager 싱글톤 리팩토링
- **상태**: 제안됨
- **설명**: Global Singleton 제거 및 DI(Dependency Injection) 적용
- **이유**: 테스트 용이성 확보

### 데이터베이스 비동기화 (Async DB)
- [ ] 비동기 쿼리 API (`QueryAsync`, `ExecuteAsync`) 구현
- [ ] DB Worker Thread Pool 연동
- [ ] `UserDB` 및 메인 로직 비동기 호출 전환

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
