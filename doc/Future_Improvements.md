# Future Improvements (기술적 야망 및 개선 과제)

서버 성능과 확장성을 극한까지 끌어올리기 위한 중장기 기술 부채 및 개선 아이디어를 기록합니다. 본 문서는 2026-01-20 실측 데이터를 기반으로 작성되었습니다.

## 1. 메모리 관리 (Memory Management)

### [ ] 고성능 글로벌 할당기 도입 (mimalloc / tcmalloc)
- **배경**: 벤치마크 결과, 람다(Tiny) 및 대형(Large) 패킷에서 사용하는 시스템 할당자(LFH)는 범용적이지만 극강의 성능이 필요할 때 병목이 될 수 있음.
- **실측 기반 통찰**: 람다 풀링 실패 사례(3.7배 느려짐)를 교훈 삼아, 억지 풀링 대신 **Thread-Local 캐시를 가진 고성능 할당기**를 도입하는 것이 차세대 정석임.
- **목표**: 프로젝트 전역에 `mimalloc` 또는 `tcmalloc`을 적용하여 힙 경합(Heap Contention)을 최소화.

### [ ] Fixed-Size Function Wrapper (무할당 람다)
- **목표**: 캡처 크기를 컴파일 타임에 제한(예: 64바이트)하여 힙 할당이 절대 발생하지 않는 고속 람다 래퍼 구현.

---

## 2. 네트워크 및 디바이스 IO (Network & IO)

### [ ] Hybrid Send Strategy (Gather Write vs Linearize)
- **배경**: 2026-01-20 실측 결과, 4KB 이하 소형 패킷은 `memcpy`가 Gather Write보다 **47% 빠름**. (Windows Kernel의 Memory Pinning 오버헤드 때문)
- **목표**: 패킷 크기가 **64KB를 초과하는 대규모 전송** 시에만 Gather Write를 활성화하는 지능형 송신 레이어 구축.
- **임계점(Threshold)**: 단일 패킷 기준 32KB~64KB 가이드를 실측 데이터로 확보함.

### [ ] 송신 큐(Send Queue) 경합성 개선 (Lock-Free)
- **배경**: 멀티스레드 송신 시 `SessionImpl` 내부 락(Spinlock) 경합 식별.
- **해결**: `moodycamel::ConcurrentQueue`와 같은 MPSC(Multi-Producer Single-Consumer) 락프리 큐 도입하여 로직 스레드 간 간섭 제거.

### [ ] Receive Buffer Zero-Copy (Chained Block)
- **목표**: 고정 크기 블록 체인 구조를 도입하여 데이터 수신부터 파싱까지 복사 횟수를 0회로 유지.

---

## 3. 로직 처리 (Logic Processing)

### [ ] Protobuf Parallel Deserialization
- **표준**: IO 스레드 풀에서 파싱을 미리 완료하고 개체화된 메시지를 로직 스레드에 전달.

---

## 4. 기타 (Miscellaneous)

### [ ] SIMD 기반 패킷 직렬화 및 검사
- **목표**: 패킷 헤더 검사나 간단한 체크섬 연산에 AVX/SSE 명령어를 적용하여 미세 최적화.
