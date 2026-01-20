# Future Improvements (기술적 야망 및 개선 과제)

서버 성능과 확장성을 극한까지 끌어올리기 위한 중장기 기술 부채 및 개선 아이디어를 기록합니다.

## 1. 메모리 관리 (Memory Management)

### [ ] 고성능 글로벌 할당기 도입 (mimalloc / tcmalloc)
- **배경**: 벤치마크 결과, 윈도우 기본 할당자(LFH)도 우수하지만 `std::function` 등의 잦은 할당 상황에서는 전용 라이브러리가 더 유리할 수 있음.
- **목표**: 프로젝트 전역에 `mimalloc` 또는 `tcmalloc`을 적용하여 힙 경합(Heap Contention)을 최소화.
- **기대 효과**: `LambdaMessage` 등 시스템 할당자를 사용하는 구간의 성능 극대화.

### [ ] Fixed-Size Function Wrapper (무할당 람다)
- **배경**: `std::function`은 캡처 크기가 작을 때 SBO(Small Buffer Optimization)를 사용하지만, 이를 넘어서면 힙 할당을 강제함.
- **목표**: 캡처 크기를 컴파일 타임에 제한(예: 64바이트)하여 힙 할당이 절대 발생하지 않는 고속 람다 래퍼 구현.
- **기대 효과**: `Dispatcher::Push` 시 발생하는 모든 힙 할당을 원천 제거.

---

## 2. 네트워크 및 디바이스 IO (Network & IO)

### [ ] Asio Gather Write (Scatter-Gather IO)
- **배경**: 현재는 여러 패킷을 보낼 때 송신 버퍼로 데이터를 모으는 `memcpy`가 발생함.
- **목표**: `asio::const_buffer` 배열을 직접 `async_write`에 넘겨 커널 레이어에서 직접 데이터를 긁어가게 함 (Zero-Copy Send).
- **상태**: 최적화 3단계 예정 과제.

### [ ] Receive Buffer Zero-Copy (Chained Block)
- **배경**: `RecvBuffer`의 선형 구조와 `memmove`에 의한 단편화 제거 오버헤드.
- **목표**: 고정 크기 블록 체인 구조를 도입하여 데이터 수신부터 파싱까지 복사 횟수를 0회로 유지.

---

## 3. 로직 처리 (Logic Processing)

### [ ] Protobuf Parallel Deserialization
- **배경**: 현재 로직 스레드에서 패킷 파싱이 집중적으로 발생.
- **목표**: IO 스레드 풀에서 파싱을 미리 완료하고 개체화된 메시지를 로직 스레드에 전달.
- **기대 효과**: 메인 루프의 틱 레이트(Tick Rate) 안정성 확보.

---

## 4. 기타 (Miscellaneous)

### [ ] SIMD 기반 패킷 직렬화 및 검사
- **목표**: 패킷 헤더 검사나 간단한 체크섬 연산에 AVX/SSE 명령어를 적용하여 미세 최적화.
