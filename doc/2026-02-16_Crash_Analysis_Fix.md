# 2026-02-16 스트레스 테스트 크래쉬 분석 및 수정 보고서

## 개요
1,000 CCU 이상의 고부하 스트레스 테스트 중 발생한 두 가지 치명적인 메모리 관련 결함(Race Condition, Use-After-Free)을 분석하고 이를 해결하기 위한 아키텍처 개선 사항을 기록함.

---

## 1. WaveManager 반복자 무효화 결함 (Race Condition)

### 🔴 문제 현상
- **증상**: 스트레스 테스트 중 `VampireSurvivorServer.exe!SimpleGame::WaveManager::Update` 루프 내에서 `Access Violation` 발생.
- **원인**: 
    - `Room::Update`가 타이머 스레드에서 실행 중일 때, 다른 스레드(핸들러 스레드)에서 플레이어의 입장/퇴장으로 인해 `WaveManager`의 상태를 변경(`Reset`, `StartSpawner` 등)함.
    - 이때 `std::vector<PeriodicSpawner> _activeSpawners`의 내부 메모리 재할당(Reallocation)이 발생하여, 업데이트 스레드가 사용 중이던 반복자(Iterator)가 무효화됨.

### ✅ 해결 방안: Strand 기반 직렬화 (Serialization)
- **변경 전**: `Room` 외부에서 멤버 함수(`Enter`, `Leave` 등)를 직접 호출하여 데이터에 병렬 접근.
- **변경 후**: **액터 모델(Actor Model)** 패턴 적용. 
    - `Room`과 관련된 모든 상태 변경 호출을 해당 방의 전용 `Strand`에 `Post` 하도록 리팩토링.
    - 실제 로직 수행 함수(`ExecuteEnter`, `ExecuteLeave` 등)를 분리하여 하나의 가상 스레드에서만 순차적으로 실행됨을 보장.
    - **효과**: 뮤텍스 락 경합 없이 완벽한 스레드 안전성 확보 및 높은 처리 성능 유지.

---

## 2. 서버 종료 시 Use-After-Free (UAF) 결함

### 🔴 문제 현상
- **증상**: Ctrl+C 등으로 서버 종료 시 `boost::asio::detail::win_iocp_io_context::cancel_timer`에서 크래쉬 발생.
- **원인**:
    - `GatewaySession`들을 관리하는 `SessionPool`이 정적(Static) 변수여서 `main` 함수 종료 이후 파괴됨.
    - 그러나 타이머가 참조하는 `io_context`(Framework 내 위치)는 `main` 함수 종료 시 먼저 사라짐.
    - "부모(iocp)가 죽었는데 자식(timer)이 종료 선언을 하려다" 잘못된 메모리를 참조하여 폭사함.

### ✅ 해결 방안: 명시적 자원 정리 시퀀스 도입
- **수정 내용**:
    - `SessionPoolBase`에 `Clear()` 메서드를 추가하여 보관된 세션 객체들을 즉시 파괴할 수 있게 함.
    - `SessionFactory::Cleanup()`을 구현하여 전역적으로 관리되는 모든 풀을 한꺼번에 정리.
    - `ServerApp::Run()`의 종료 시퀀스 최상단에 `SessionFactory::Cleanup()` 호출을 배치하여, `io_context`가 살아있는 동안 모든 하위 객체가 먼저 안전하게 파괴되도록 보정.

---

## 3. 소멸자 내 shared_from_this 호출 (bad_weak_ptr) 결함

### 🔴 문제 현상
- **증상**: 서버 종료 시 `std::bad_weak_ptr` 예외 발생과 함께 서버 강제 종료.
- **원인**: 
    - `Room` 객체의 소멸자(`~Room`)에서 `Stop()`을 호출함.
    - `Stop()` 내부에서 비동기 처리를 위해 `shared_from_this()`를 호출하여 수명 연장 시도.
    - 이미 참조 횟수가 0이 되어 소멸 중인 객체는 `shared_ptr`로 부활할 수 없다는 C++ 표준 위반으로 예외 발생.

### ✅ 해결 방안: 동기/비동기 정리 로직의 완전 분리
- **수정 내용**:
    - **`InternalClear()` 도입**: `shared_from_this`를 쓰지 않는 순수 동기식 자원 정리 함수(타이머 취소 등)를 분리.
    - **소멸자 수정**: 소멸자에서는 `Stop()` 대신 `InternalClear()`만 호출하도록 변경.
    - **셧다운 지연(Rejection Trap) 해결**: 
        - `Framework::Stop()`에서 `ThreadPool`을 즉시 끄지 않고, `RoomManager` 정리가 완전히 끝날 때까지 "일감을 받을 수 있는 상태"로 유지.
        - 모든 관객(Room)이 퇴장한 뒤에 비로소 `Framework::Join()`에서 스레드 풀 전원을 차단.
    - **즉각 반응**: 무거운 게임 루프(`ExecuteUpdate`) 시작 시 `isStopping` 체크를 넣어 큐에 남은 작업을 즉시 탈출.

---

## 3. 결론 및 성과
- 고부하 상황에서 서버 생명주기(Life-cycle)와 런타임 로직 간의 스레드 충돌 문제를 아키텍처적으로 해결함.
- **Lock-Free**에 가까운 설계를 통해 1,000 CCU 환경에서도 크래쉬 없이 안정적인 게임 루프 유지를 확인하였으며, 정상 종료 시의 안정성까지 확보함.
