# Project TODO List

## 🚀 Tasks In-Progress & Remaining

### Critical Optimization Tasks (Next Sprint)
- [x] **[SOLVED] NATS Driver Memory Leak (Confirmed)**
    - **문제**: `Subscribe` 시 할당한 콜백 래퍼(`persistentCallback`)가 해제되지 않음 (확정 누수).
    - **해결**: 구독 해지 및 객체 소멸 시 반드시 `delete`하도록 추적 벡터 `m_callbacks`를 추가하여 수정.
- [x] **[SOLVED] CommandConsole Shutdown Safety (UAF Risk)**
    - **문제**: `Stop()` 시 `detach()` 사용으로 인해 메인 프로세스 종료 후에도 스레드가 살아있어 UAF 위험.
    - **해결**: `std::shared_ptr<SharedState>`를 도입하여 스레드 생명주기와 객체 캡처 무결성을 보장(UAF 원천 차단).
- [x] **[SOLVED] Multi-Level MessagePool 확장**
    - **사전 작업 (Data-Driven Profiling)**: 
        - `MessagePool::AllocatePacket` 내부에 `IMetrics`를 연동하여 패킷 사이즈별(ex: 1KB 이하, 4KB 이하, 힙 할당) 할당 빈도 카운팅 (`GetMetrics().GetCounter()->Increment()`).
        - 기존 통합되어 있는 WebSocket 서버를 활용하여 외부 의존성(Grafana 등) 없이 가벼운 실시간 웹 모니터링 대시보드(HTML/JS)로 메트릭(JSON)을 푸시(Push)하여 실제 빈도 및 병목 눈으로 확인하기.
    - **목표**: 프로파일링 결과를 바탕으로 4KB 초과 시 힙 할당을 방지하고, 대형 패킷(9KB)을 효율적으로 처리하기 위한 계층형 풀 크기 산정 및 도입.
    - **스펙**:
        - **Small Pool (1KB)**: 빈번한 소형 패킷(이동, 스탯) 및 일반 UDP용 (`UDP_MAX_APP_BYTES` $\approx$ 1.2KB).
        - **Large Pool (16KB)**: 몬스터/플레이어 동기화 패킷(약 9KB) 및 KCP 재조립용.
    - **전략**: `AllocatePacket`에서 사이즈 분기에 따라 적절한 풀에서 블록을 가져오도록 변경.
- [ ] **[CRITICAL] SpatialGrid & Room Loop 성능 최적화 (Lock Contention 해소)**
    - **문제**: `SpatialGrid::Query`가 루프 내부에서 `objMgr.GetObject(id)`를 호출하며 수천 번의 Mutex Lock 점유 발생. 또한 한 틱에 `GetAllObjects()`를 과도하게 호출하여 리스트 복사 부하 가중.
    - **해결**:
        - `SpatialGrid`가 ID 대신 `::System::RefPtr<GameObject>`를 직접 들고 있도록 수정 (Lock 제거).
        - `Room::ExecuteUpdate` 시작 시 `GetAllObjects()` 결과를 한 번만 획득하여 하위 시스템으로 전파.
    - **상태**: 리팩토링 후 성능 저하 보고됨, 즉시 수정 필요.
- [ ] **[CRITICAL] Lock-Free Entity Pooling (SimplePool 교체 & std::bad_weak_ptr 크래시 해결)**
    - **상태**: `::System::RefPtr` 도입을 통한 1단계 마이그레이션 완료 (Player, Monster, Projectile 등).
    - **원인**: 현재 `PlayerFactory` 등이 `SimplePool` 사용 중 객체를 `std::shared_ptr` 커스텀 딜리터로 풀에 반환함. 이 과정에서 `std::enable_shared_from_this`의 내부 컨트롤 블록(`weak_ptr`)이 파괴/만료된 상태로 메모리가 재사용되어, `RefreshInventoryEffects` 등에서 `shared_from_this()` 호출 시 `std::bad_weak_ptr` CommandConsole 입력 시 크래시 발생 확인.
    - **해결**: `std::shared_ptr` 대신 외부 힙 할당이 없는 침투식 포인터인 `::System::RefPtr`을 도입하여 컨트롤 블록 한계 극복.
- [ ] **[HIGH] GatewaySession 버퍼 최적화 (Zero-fill 제거)**
    - 수신 버퍼 `resize()` 시 발생하는 불필요한 0 초기화 제거 (`reserve` + `push_back` 활용).
    - 초당 수십 GB의 메모리 쓰기 부하 제거.

- [ ] **[HIGH] UDPSession Partial Hot Path Optimization**
    - **참조**: `doc/code_review/2026_02_19_System_Review.md`
    - **파일**: `src/System/Session/UDPSession.cpp:232`
    - **문제**: `SendReliable`에서 대형 패킷(>1KB) 전송 시 `std::vector` 힙 할당 발생.
    - **영향**: 대형 패킷 빈번 전송 시 힙 할당/해제 부하 증가 (Partial Hot Path Violation).
    - **해결**: `MessagePool` 또는 `Block Allocator`를 사용하여 Zero-Copy 보장.

- [ ] **[LONG-TERM] Smart Packet Builder (Auto-Chunking Helper)**
    - **목표**: 개발자가 패킷 크기를 신경 쓰지 않고 데이터를 밀어 넣으면, 프레임워크가 알아서 안전한 크기(Chunk)로 잘라서 보내주는 헬퍼 도입.
    - **이유**: 현재는 `Room_Update.cpp`처럼 개발자가 직접 루프를 쪼개야 함(Human Error 가능성).
    - **구현 아이디어**:
        ```cpp
        // Usage Example
        PacketBuilder<S_MoveObjectBatch> builder(objects);
        builder.ProcessInChunks(300, [](auto& packet, auto& obj) { ... }, BroadcastFunc);
        ```

## Essential Features (Framework Missing)
- [ ] **[SOLVED] Crash Dump Handler (Activation Check)**
    - **상태**: `src/System/Debug/CrashHandler`에 구현 확인됨.
    - **작업**: `ServerMain.cpp`에서 `CrashHandler::Init()`가 호출되는지 확인하고, 테스트 크래시로 덤프 생성 검증.
- [ ] **[MEDIUM] Config Hot-Reload**
    - **목표**: 서버 중단 없이 설정 파일(`DesignData`)을 리로드.
    - **필요성**: 라이브 서비스 중 밸런스 패치(예: 몬스터 스펙 조정) 대응.
- [ ] **[LOW] IP Ban / Filter**
    - **목표**: 악성 클라이언트 접속 차단 계층(DDOS 방어용).
- [ ] **[LOW] ThreadPool Graceful Shutdown**
    - **목표**: 서버 종료 시(`Stop()`) 대기 중인 작업(DB 저장 등)을 모두 처리하고 안전하게 종료.
    - **이유**: 현재 `ThreadPool`은 즉시 종료(`return`)하여 데이터 유실 가능성 있음.
- [ ] **[MEDIUM] Metrics Optimization**
    - **문제**: `GetCounter` 호출 시마다 `mutex` 잠금.
    - **해결**: 카운터 포인터를 멤버 변수로 캐싱하여 사용.

## Refactoring & Cleanup (Structure)
- [ ] **[HIGH] main.cpp 추상화 위반 해결 (Implementation Header 제거)**
    - **문제**: `main.cpp`에서 `NetworkImpl.h`, `WebSocketNetworkImpl.h` 등 구현체 헤더를 직접 참조하고 있음.
    - **원인**: 실시간 메트릭 브로드캐스트를 위해 `INetwork`에서 구현체로 `dynamic_pointer_cast`를 수행하기 때문.
    - **해결**: `IFramework` 또는 `INetwork` 인터페이스에 메트릭 브로드캐스트 기능을 추상화하여 제공하도록 수정.
- [ ] **[MEDIUM] Flatten Dispatcher Folder**
    - **목표**: `src/System/Dispatcher/DISPATCHER` 폴더 내용을 상위로 이동하고 빈 폴더 삭제.
- [ ] **[LOW] Redistribute Root Interfaces**
    - **목표**: `src/System/I*.h` 파일들을 각 기능별 서브디렉토리로 이동하여 응집도 향상.
- [ ] **[LOW] Merge Database Drivers**
    - **목표**: `src/System/Drivers` 폴더를 `src/System/Database/Drivers`로 이동하여 역할 명확화.

## Framework Promotion Candidates (From Game Server)
- [ ] **[HIGH] Promote Vector2**
    - **대상**: `src/Examples/VampireSurvivor/Server/Math/Vector2.h`
    - **목표**: `src/System/Math/Vector2.h`로 이동하여 프레임워크 표준 타입으로 사용.
- [ ] **[MEDIUM] Generalize SpatialGrid**
    - **대상**: `src/Examples/VampireSurvivor/Server/Game/SpatialGrid.h`
    - **목표**: `GameObject` 의존성을 제거하고 템플릿(`SpatialHashGrid<T>`)으로 일반화하여 `src/System/Algorithm`으로 이동.
- [ ] **[MEDIUM] Generalize BehaviorTree**
    - **대상**: `src/Examples/VampireSurvivor/Server/Entity/AI/BehaviorTree/BehaviorTree.h`
    - **목표**: `Monster/Room` 의존성을 `Context` 템플릿으로 추상화하여 `src/System/AI`로 이동.

---







---

## 🔧 Technical Debt (from Weapon Level-Up Mechanics Work)

### Critical
    - **후속 조치**: 지속적인 모니터링. 재발 시 `PacketBase.h` define 활성화.

- [x] **[SOLVED] Protobuf 직렬화 크래시 (Integer Truncation & Buffer Overflow)**
    - **완료**: 2026-02-19
    - **원인**: `PacketHeader::size`(uint16_t) 한계를 넘는 87KB 패킷 생성 → 21KB로 잘림 → 힙 오버플로우.
    - **해결**: `PacketBase` MaxSize(65535) 제한 및 `Room_Update` 청크 분할(300개) 적용.
    - **상세**: `doc/learning/2026_02_19_Protobuf_Crash_Analysis.md` 참조.

- [x] **[SOLVED] CombatManager 레벨 인덱스 접근 버그 수정 (Dense Array Optimization)**
    - **파일**: `src/Examples/VampireSurvivor/Server/Game/CombatManager.cpp:161` 등 다수
    - **원인**: 기획 데이터(JSON)에 비어 있는 레벨(Sparse level array)이 존재할 때 `weapon.levels[playerWeaponLevel - 1]`로 접근 시 Out-of-Bounds 크래시 위험.
    - **해결**: `DataManager`에서 무기/패시브 기획 데이터를 로드할 때 누락된 레벨이 있으면 즉시 프로그램 종료(Assert 기능)하도록 검증 로직 추가. 이를 통해 모든 레벨 데이터가 1-indexed 오름차순 Dense Array임을 런타임에 완벽히 보장하여 `O(1)` 직접 접근의 안전성 확보 및 성능 향상 완료.

### Warning
- [ ] **[MEDIUM] Projectile::_hitTargets 벡터 무한 증가 방지**
    - **파일**: `src/Examples/VampireSurvivor/Server/Entity/Projectile.h`
    - **문제**: 무한 관통 투사체(`pierce = -1`)가 `_hitTargets`에 계속 ID 추가
    - **영향**: 장시간 생존하는 Orbit 투사체에서 메모리 무한 증가
    - **해결**: 최대 크기 제한 또는 순환 버퍼 사용 고려

- [ ] **[MEDIUM] EffectManager 몬스터 사망 시 effect 정리 확인**
    - **파일**: `src/Examples/VampireSurvivor/Server/Game/Effect/EffectManager.cpp`
    - **문제**: 몬스터 사망 시 해당 몬스터의 effect가 map에서 제거되는지 확인 필요
    - **영향**: 메모리 누수 가능성
    - **확인**: `Monster::OnDeath` → EffectManager 정리 로직 점검

### Info / Documentation
- [ ] **[LOW] Orbit speedMult 의미 문서화**
    - **파일**: `src/Examples/VampireSurvivor/Server/Game/DamageEmitter.cpp:317`
    - **내용**: Orbit의 `speedMult`는 각속도(rad/s), Linear의 `speedMult`는 선형 속도
    - **조치**: 무기 데이터 작성 가이드에 명시 필요
    - 조치: 필드명 변경 또는 주석 추가 고려

---

## 🧪 QA & Testing Infrastructure (From Code Review)

### Priority Tests
- [ ] **[HIGH] MessagePool 사후 모니터링 추적 (monitor.html)**
    - **목표**: Multi-Level MessagePool 확장에 따른 메모리 할당 분포 추적 및 Heap 의존성 감지.
    - **📌 핵심 모니터링 지표**:
        - `msgpool_alloc_heap` (OS Heap Fallback 횟수): **1 이상이면** 16KB를 초과하는 초과 스펙의 대형 패킷이 발생했다는 의미 (즉각적인 최적화/청크 분할 대상).
        - `msgpool_heap_bytes` (총 힙 할당 바이트): 허용 범위를 넘은 패킷으로 인한 메모리 오버헤드 측정.
        - **풀 별 부하 측정**: `<1KB`, `<4KB`, `<16KB` 메트릭을 주시하여 병목이 발생하는 풀 사이즈가 있는지 주기적으로 진단.

- [ ] **[HIGH] UDP Session Lifecycle Test (`UDPSessionTest.cpp`)**
    - **목표**: 세션 생성부터 소멸까지의 참조 카운트(RefCnt) 및 상태 변화 완전 검증.
    - **시나리오**:
        1. `CreateSession` 및 초기 상태 확인.
        2. 패킷 수신/송신 시 `IncRef`/`DecRef` 대칭성 검증.
        3. `Timeout` 또는 `Close` 호출 시 `Registry` 해제 및 객체 소멸 확인.
        4. `SessionFactory` 재사용(Pooling) 시 오염된 상태(이전 데이터) 잔존 여부 확인.

- [ ] **[MEDIUM] NAT Rebinding Simulation**
    - **목표**: 모바일 환경에서 IP/Port 변경 시 세션 끊김 없이 유지되는지 검증.
    - **구현**:
        1. Client A(`1.2.3.4:1000`)가 `TokenX`로 연결.
        2. Client A가 `Registry`에 등록됨 확인.
        3. 동일한 `TokenX`로 Client A(`1.2.3.4:2000`)가 패킷 전송.
        4. `Registry`가 새로운 Endpoint로 갱신되고 `Session` 포인터가 유지되는지 검증.

- [ ] **[MEDIUM] Memory Leak Detection Automation**
    - **목표**: 단위 테스트 및 서버 종료 시 누수 자동 감지.
    - **구현**:
        1. MSVC `_CrtSetDbgFlag` 활성화 (Debug 빌드).
        2. `SimplePool`, `MessagePool` 등의 `Alloc/Free` 불일치 추적 카운터 추가.
        3. 서버 종료 시 `ObjectCounter`가 0이 아닌 객체 덤프 기능 구현.

- [ ] **[LOW] BackendSession Scatter-Gather Verification**
    - **목표**: `BackendSession`에서 `std::vector<const_buffer>`를 사용한 Scatter-Gather 전송이 실제로 복사 없이 이루어지는지 패킷 캡처 또는 메모리 프로파일링으로 검증.

- [ ] **[QA] Automated Stress Test Runner Implementation**
    - **참조**: `src/Tools/StressTest/StressTestClient.cpp` (단일 클라이언트 로직 존재)
    - **문제**: 현재 StressTestClient는 단일 연결 및 기본 동작만 구현되어 있음. `doc/implementation_plan_kr.md`에서 언급된 3,000 CCU 부하 생성기가 부재함.
    - **구현**:
        1. 다중 스레드 기반의 클라이언트 오케스트레이터 구현 (`StressClientManager`).
        2. Config 기반 CCU 조절, 램프업(Ramp-up) 시나리오, 패킷 빈도 조절 기능.
        3. 실시간 TPS 및 응답 지연(Latency) 통계 수집기.

> **Note**: 위 테스트 구현 완료 시 `doc/code_review_issues.md`의 "테스트 추가 필요" 섹션을 ([x])로 마크하고 업데이트하십시오.
- [ ] **[LOW] Zone maxTargets 의미 명확화**
    - **파일**: `src/Examples/VampireSurvivor/Server/Game/DamageEmitter.cpp:357-358`
    - **내용**: Zone에서 `maxTargets`는 "타격할 몬스터 수"가 아니라 "번개 횟수"로 사용
    - **조치**: 필드명 변경 또는 주석 추가 고려


테스트로 찾은문제
1. 툴팁이 스킬/패시브표시 패널보다 ZIndex 가 작은것같음 뒤에 표시됨
2. 회전하는 스킬의 중심점이 캐릭터가 움직일때 캐릭터의 뒤에서 쪼차오는식으로 보임. 사용자경험 안좋음
3. 번개스킬 비쥬얼라이징 필요함. 
4. 여전히 끝판왕은 몬스터가 유저쫒아오는것 해결하기임.
5. 맵을 만들기는 해야할듯 => 맵을 조밀하게만들고. 물리효과를 좀더 키워야할까.
6. 스킬남발 + 과부하시 protobuf serialize  에러발생.



