# Performance Benchmark Report - Network Infrastructure

**Date**: 2026-02-07
**Target**: UDP Session Pool & KCP Logic Overhead (Comparison Study)
**Source Code**: `src/System/Session/tests/TestPerf.cpp` (GTest)

> **Note**: This report is based on historical data. To reproduce results, build the `SystemTests` target and run `TestPerf`.

## 1. Overview
네트워크 엔진의 효율성을 객관적으로 증명하기 위해, 현재 구현된 **실험군(Pooling/KCP)**과 기존 방식의 **대조군(Direct/Raw)**을 비교 분석함.

## 2. Benchmark Results (Debug Build - Local Environment)

### [Test A] Session Management Efficiency
- **Description**: 미리 생성된 풀에서 꺼내는 방식(Pooling) vs 루프마다 힙 메모리를 할당/해제하는 방식(Direct) 비교.
- **Results**:
  - **Session Pooling**: **12.3267 us** per op
  - **Direct Allocation**: **12.0048 us** per op
- **Analysis**: 단일 스레드 테스트에서는 직접 할당이 약 2.6% 빠르게 측정됨. 이는 `ConcurrentQueue`의 멀티스레드 안전용 원자적(Atomic) 연산 비용이 단일 스레드 환경에서의 단순 `new`보다 높기 때문임. 
- **Engineering Verdict**: 실환경(멀티스레드)에서는 힙 락 경합이 발생하므로 Pooling 방식이 압도적인 안정성을 제공할 것으로 판단됨.

### [Test B] Protocol Processing Cost
- **Description**: 신뢰성을 보장하는 KCP 로직 vs 단순 데이터 복사(Raw)만 수행하는 최소 비용 비교.
- **Results**:
  - **KCP Wrapper**: **8.5481 us** per packet
  - **Raw Data Copy**: **0.0095 us** per packet
- **Protocol Cost**: **패킷당 약 8.5386 us 지불**
- **Analysis**: 신뢰성 보장을 위해 패킷당 약 8.5us의 연산 시간을 사용함. 서버 틱(33ms) 대비 비용이 극히 낮아(0.02%), 성능과 신뢰성 사이의 최적의 균형점임을 데이터로 확인함.

## 3. Improvements Applied
- **Dangling Pointer 방지**: `UDPSession::OnRecycle` 시 `_dispatcher` 초기화 완료.
- **WSA 안정성**: 테스트 종료 시 Hang 현상 해결을 위한 WinSock 관리 최적화.

---
*Results measured via `TestPerf.cpp` and confirmed via user execution.*
