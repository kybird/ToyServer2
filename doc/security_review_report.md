# [리뷰 보고서] 고성능 락프리 C++ 서버 프로젝트 보안 및 안정성 검토

## 1. 개요
`ToyServer2` 프로젝트의 핵심 시스템(Memory, Network, Session)을 분석한 결과, 고성능을 지향하고 있으나 **멀티스레드 환경에서의 데이터 오염(Data Corruption)** 및 **수명 주기 관리(Lifetime Management)** 측면에서 치명적인 결함이 발견되었습니다. 특히 ASIO의 멀티스레드 모델을 사용하면서도 `strand`를 통한 동기화가 설계되지 않은 점이 가장 큰 위험 요소입니다.

---

## 2. 중점 검토 항목별 발견 사항 (소스 코드 기반 검증)

### 2.1 Lock-free 구조 및 원자성 (Memory Model)

#### [위험 요소 1] 통계 atomic 변수의 False Sharing (캐시 라인 간섭)
*   **원인 코드:** `src/System/Memory/LockFreeObjectPool.h` (Line 132-137)
    ```cpp
    private:
        moodycamel::ConcurrentQueue<T *> _pool;
        // Stats
        std::atomic<size_t> _allocCount{0}; // Total Allocated (System Memory)
        std::atomic<size_t> _poolCount{0};  // Total Idle (Cache)
    ```
*   **검토 내용:** `_allocCount`와 `_poolCount`는 서로 다른 스레드에서 `Pop()`/`Push()` 호출 시 매우 빈번하게 업데이트됩니다. 두 변수가 메모리 상에서 인접해 있어 캐시 라인(64B)을 공유할 확률이 높으며, 이는 멀티코어 환경에서 불필요한 캐시 라인 무효화를 유발하여 L1/L2 캐시 적중률을 심각하게 떨어뜨립니다. (성능 저하의 주요 원인)

---

### 2.2 ASIO 비동기 메커니즘

#### [위험 요소 2] Strand 설계 부재 및 커스텀 동기화 패턴의 위험성
*   **원인 코드 (커스텀 락프리 소유권):** `src/System/Session/Session.cpp` (Line 101) / `GatewaySession.cpp` (Line 209-228)
    ```cpp
    // Session::EnqueueSend
    if (_isSending.exchange(true, std::memory_order_acq_rel) == false) { Flush(); }
    
    // GatewaySession::Flush (Straggler check)
    if (_sendQueue.try_dequeue(straggler)) {
        if (!_isSending.exchange(true)) { Flush(); } 
    }
    ```
*   **검토 내용:** 성능 최적화를 위해 의도적으로 `asio::strand`를 제거하고 원자적 플래그(`_isSending`)를 이용한 **커스텀 소유권 체인**을 구현한 흔적이 있습니다.
    - **긍정적 측면:** `strand` 작업 예약(`post`) 오버헤드를 줄이고 배치(Batching) 처리를 극대화하려는 설계 의도가 보입니다.
    - **위험 요소:** 위와 같은 수동 동기화는 구현이 매우 복잡하여 실수가 발생하기 쉽습니다. 특히 `OnWriteComplete` 핸들러가 실행되는 시점과 새로운 패킷이 인큐되는 시점이 절묘하게 겹칠 경우, `_linearBuffer`에 대한 중복 접근(Race Condition)을 100% 차단한다는 보장이 어렵습니다. 또한 암호화(`AesEncryption`) 로직이 스레드 세이프를 위해 멤버 변수를 수정하지 않고 매번 IV를 복사해서 쓰도록 설계되었으나, 이는 **IV 재사용**이라는 보안적 취약점으로 이어집니다.

#### [위험 요소 3] 객체 풀링과 비동기 I/O 간의 ABA 문제
*   **원인 코드 (즉시 반납):** `src/System/Session/SessionPool.h` (Line 80-87: `Release()`)
    ```cpp
    void Release(T *session) {
        if (session) {
            _pool.enqueue(session); // 진행 중인 IO 확인 없이 즉시 큐에 삽입
            _availableCount.fetch_add(1);
        }
    }
    ```
*   **검토 내용:** 세션 연결이 끊겼을 때 `Release()`가 호출되어 객체가 풀로 돌아가도, OS 레벨에서 처리 중이던 `async_read` 핸들러는 취소되지 않고 나중에 완료될 수 있습니다. 만약 해당 메모리가 다른 연결에 재할당된 후 핸들러가 실행되면, `_recvBuffer` 등 내부 상태를 오염시켜 예기치 못한 동작(Logical ABA)을 유발합니다. 이는 **성능 최적화와 무관한 심각한 버그**이며 반드시 `_ioRef` 검증 후 반납하도록 수정되어야 합니다.

---

### 2.3 멀티플랫폼 호환성 및 표준

#### [위험 요소 4] 예외 처리 미비로 인한 워커 쓰레드 중단
*   **원인 코드:** `src/System/Framework/Framework.cpp` (Line 285: `Run()`)
*   **검토 내용:** 개별 비동기 핸들러 내부(예: 패킷 파싱 로직)에서 예외가 발생하면 예외가 핸들러를 탈환하여 `run()` 루프 자체를 중단시킵니다. 이는 특정 서비스 가용성을 영구적으로 위협하는 요소입니다.

---

### 2.4 리소스 및 성능 최적화

#### [위험 요소 5] 과도한 시스템 쓰레드 생성 (Object Pool)
*   **원인 코드:** `src/System/Session/SessionPool.h` (Line 99: `TriggerBackgroundGrowth()`)
*   **검토 내용:** 풀 부족 시 매번 `std::thread`를 생성하여 분리(`detach`)하는 방식은 부하 상황에서 시스템 리소스 급증을 초래합니다. 기존 `_threadPool`을 활용한 비동기 확장이 권장됩니다.

---

## 3. 종합 의견 및 우선순위

| 우선순위 | 항목 | 위험도 | 조치 필요성 |
| :--- | :--- | :--- | :--- |
| **P0** | **Session Pool ABA** | **치명적** | 객체 재사용 시점의 안전성 보장 안됨 (버그) |
| **P1** | **커스텀 동기화 레이스** | **높음** | `strand` 대신 사용된 수동 동기화의 검증 필요성 |
| **P1** | **False Sharing** | **높음** | 원자적 변수 간섭으로 인한 성능 병목 |
| **P2** | **IV 재사용 취약점** | **보통** | 동기화를 피하기 위한 우회 설계의 부작용 |
| **P2** | **워커 쓰레드 예외 관리** | **보통** | 서비스 가용성 위협 |
