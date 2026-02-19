# System 프레임워크 코드 리뷰 보고서

**작성일**: 2026-02-19
**검토 범위**: `src/System` (Core Framework)

---

## 1. 개요 (Overview)
`doc/coding_convention.md`에 명시된 "고성능 MMORPG 서버 프레임워크"의 기준에 따라 `src/System` 내부 코드의 적합성, 잠재적 성능 문제, 코딩 규약 위반 여부를 검토하였습니다.
전반적으로 구조적 분리와 의존성 관리가 훌륭하게 되어 있으나, **Hot Path(고성능 구간)에서의 메모리 할당 정책 위반**이 일부 발견되었습니다.

---

## 2. 주요 발견 사항 (Key Findings)

### 🚨 [CRITICAL] UDPSession::SendReliable 힙 할당 (Hot Path Violation)
- **위치**: `src/System/Session/UDPSession.cpp` (Line 232)
- **문제 코드**:
    ```cpp
    std::vector<uint8_t> buffer(size); // <-- HEAP ALLOCATION & ZERO FILL
    pkt.SerializeTo(buffer.data());
    ```
- **심각도**: **높음**
- **설명**: `SendReliable`은 전투 중 가장 빈번하게 호출되는 함수 중 하나입니다. 패킷 크기가 1024바이트를 넘을 때마다 `std::vector`를 생성하여 힙 메모리를 할당하고 해제합니다.
- **위반**: `doc/coding_convention.md` 섹션 4.2.2 "동적 메모리 할당 금지" 위반.
- **권장 수정**: `ThreadLocal` 버퍼를 사용하거나, `MessagePool`에서 `Large Block`을 대여하여 직렬화 후 전송.

### ⚠️ [HIGH] GatewaySession TCP 버퍼 제로필 (Zero-Fill Overhead)
- **위치**: `src/System/Session/GatewaySession.cpp` (Line 240)
- **문제 코드**:
    ```cpp
    _impl->_linearBuffer.clear();
    _impl->_linearBuffer.resize(totalSize); // <-- ZERO FILL (O(N) memset)
    ```
- **심각도**: **중간** (대역폭이 높을수록 심각해짐)
- **설명**: `std::vector::resize()`는 새로 늘어난 영역을 0으로 초기화합니다. 초당 수십 MB~GB 데이터를 처리할 때, 이 초기화 비용은 순수한 CPU 낭비입니다.
- **권장 수정**:
    - `resize` 대신 `unsafe_resize` 패턴 사용 (커스텀 컨테이너).
    - 또는 `capacity` 내에서는 `size`만 변경하고 직접 `memcpy` 사용.

### ⚠️ [MEDIUM] MessagePool의 대형 패킷 처리 부재
- **위치**: `src/System/Dispatcher/MessagePool.cpp` (Line 36)
- **문제 코드**:
    ```cpp
    // 4KB 초과 시
    void *block = ::operator new(allocSize); // <-- RAW NEW
    ```
- **설명**: 4KB를 초과하는 패킷(예: `S_RoomList`, `S_MoveObjectBatch`)이 발생할 때마다 OS 힙 할당이 발생합니다. MMO 특성상 이런 패킷은 주기적으로 버스트(Burst)하게 발생하므로 단편화(Fragmentation)를 유발할 수 있습니다.
- **권장 수정**: `Multi-Level Pool` 도입 (1KB, 4KB, 16KB 등 크기별 풀링).

### ℹ️ [INFO] Dispatcher::Push의 new 사용
- **위치**: `src/System/Dispatcher/DISPATCHER/DispatcherImpl.cpp` (Line 328)
- **내용**: `LambdaMessage` 할당 시 `new` 사용 (주석: LFH가 풀링보다 빠르다는 벤치마크 결과에 따름).
- **의견**: 합리적인 최적화 선택으로 보이나, OS 할당자에 전적으로 의존하는 것은 예측 불가능성(Latency Spike)을 내포합니다. 향후 `SmallObjectAllocator` 도입 고려.

---

## 3. 구조적 강점 (Good Practices)
- **Interface Separation**: `IDispatcher.h`와 `DispatcherImpl`의 분리가 명확하여 컴파일 의존성을 최소화함.
- **Lock-Free Queue**: `moodycamel::ConcurrentQueue`를 적재적소에 사용하여 컨텐션을 최소화함.
- **Zero-Copy Intent**: `UDPSession` 수신부 등에서 가능한 한 복사를 줄이려는 시도(`MessagePool::AllocatePacket` 활용)가 보임.

---

## 4. 미구현 필수 기능 (Missing Features)
MMORPG 프레임워크로서 다음 기능들의 부재가 확인되었습니다.
1.  **Crash Dump Handler**: 서버 크래시 시 미니덤프(.dmp)를 남기는 핸들러.
2.  **Metrics System**: 패킷 처리량, 큐 대기열 길이, RTT 등을 실시간으로 수집/노출(Prometheus 등)하는 시스템.
3.  **Config Hot-Reload**: 서버 중단 없이 설정 파일(`DesignData`)을 리로드하는 기능.
4.  **IP Ban / Filter**: DDOS 방어용 IP 필터링 계층.

---

## 5. 결론 및 제안
현재 프레임워크는 구조적으로 견고하나, **대규모 트래픽 처리를 위한 메모리 관리(Zero-Overhead)** 측면에서 개선이 필요합니다.
우선순위에 따라 **UDPSession 힙 할당 제거**와 **GatewaySession 버퍼 최적화**를 최우선으로 진행해야 합니다.
