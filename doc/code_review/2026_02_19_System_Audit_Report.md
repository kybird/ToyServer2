# System 프레임워크 종합 감사 보고서 (Code & Structure Audit) - Revised

**작성일**: 2026-02-19 (Updated based on Critique)
**대상**: `src/System` 전체 코드 및 디렉토리 구조
**감사자**: Antigravity Smart Agent

---

## 1. 종합 요약 (Executive Summary)

`src/System` 프레임워크의 상세 감사 결과, 기능적 완성도는 높으나 **메모리 누수(Memory Leak)**와 **스레드 안전성(Thread Safety)** 측면에서 치명적인 결함이 발견되었습니다. 또한, 디렉토리 구조의 정리 정돈이 미흡하여 유지보수성을 저해하고 있습니다.

*   **코드 품질 (Logic)**: ⭐⭐⭐⭐ (4.0/5.0) - *Zero-Copy 인프라는 훌륭하나, MQ/Console에 치명적 버그 존재*
*   **구조 정돈 (Structure)**: ⭐⭐⭐ (3.0/5.0) - *중첩 폴더, 인터페이스 산재 등 정리 필요*
*   **안정성 (Stability)**: ⭐⭐⭐ (3.0/5.0) - *Nats 누수 및 Console UAF 위험*

---

## 2. 모듈별 상세 분석 (Module Analysis)

### 2.1. Foundation & Network
*   **Timer & Log**: `TimingWheel`과 `Async Logger`는 최적화 수준이 매우 높음.
*   **UDP Network**: `Zero-Copy` 인프라는 훌륭함. 단, `UDPSession::SendReliable`에서 대형 패킷 전송 시 `std::vector` 힙 할당이 발생하여 부분적인 성능 저하 유발 (**Partial Hot Path Violation**).
*   **CrashHandler**: `System/Debug/CrashHandler.cpp`에 구현되어 있으며, `ServerMain` 등에서 초기화 호출도 확인됨. (기존 보고서 정정)

### 2.2. Data & Utility
*   **Database**: `Async Transaction` 패턴이 우수하며 `RAII` 커넥션 풀링도 안전함.
*   **MQ (`NatsDriver`)**: **[CRITICAL]** `Subscribe` 시 할당한 `persistentCallback` 힙 메모리를 해제하는 로직이 전무함. 이는 실행 시간이 길어질수록 메모리가 무한히 증가하는 **확정적 메모리 누수(Confirmed Memory Leak)**임.
*   **Console (`CommandConsole`)**: **[CRITICAL]** `Stop()` 시 스레드를 `detach()`하고 있어, 메인 프로세스 종료 시 `InputLoop`가 살아있는 상태에서 소멸된 객체에 접근할 위험(**Use-After-Free**)이 있음.

### 2.3. Structure (Directory)
*   **중복 중첩**: `src/System/Dispatcher/DISPATCHER` 폴더 존재. (정리 필요)
*   **루트 오염**: `I*.h` 인터페이스 파일들이 루트에 산재함.
*   **역할 모호**: `Drivers` 폴더가 `Database`와 분리되어 있어 구조적 혼란 야기.

---

## 3. 핵심 개선 권고 사항 (Critical Recommendations)

### 🚨 Priority 0: 치명적 버그 수정 (Safety First)
1.  **NatsDriver Memory Leak Fix**: 구독 해제 및 객체 소멸 시 `persistentCallback`을 반드시 `delete`하도록 수정.
2.  **CommandConsole Shutdown Safety**: `detach()` 제거 및 `join()`으로 변경. `std::getline` 블로킹 문제를 해결하기 위해 비동기 입력이나 `CancelIo` 고려.

### ⚠️ Priority 1: 성능 병목 해결 (Optimization)
1.  **UDPSession Heap Allocation**: `SendReliable`의 대형 패킷 경로에서 `std::vector` 대신 `MessagePool` 또는 `Block Allocator` 사용.
2.  **Metrics Optimization**: `GetCounter` 호출 시 락 경합을 피하기 위해, 카운터 객체를 멤버 변수로 캐싱(`shared_ptr`)하여 사용.

### 🧹 Priority 2: 디렉토리 구조 정리 (Cleanup)
1.  **Flatten Dispatcher**: `System/Dispatcher/DISPATCHER` 폴더 제거 및 파일 상위 이동.
2.  **Move Interfaces**: 루트의 `I*.h` 파일들을 각 기능별 서브디렉토리로 이동.
3.  **Merge Drivers**: `System/Drivers` -> `src/System/Database/Drivers`로 이동.

---

## 4. 결론 (Conclusion)

인프라의 성능(`Zero-Copy`)은 훌륭하지만, **안정성(Leak/Crash)**을 위협하는 요소들이 발견되었습니다.
특히 `NatsDriver`의 누수와 `CommandConsole`의 UAF 위험은 서버 장기 운영 시 치명적이므로 **즉시 수정(Immediate Fix)**이 필요합니다.
구조적 정리(Cleanup)는 그 이후에 진행해도 늦지 않습니다.
