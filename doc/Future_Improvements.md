# Future Improvements (기술적 야망 및 개선 과제)

서버 성능과 확장성을 극한까지 끌어올리기 위한 중장기 기술 부채 및 개선 아이디어를 기록합니다. 본 문서는 2026-01-20 실측 데이터를 기반으로 작성되었습니다.

## 1. 메모리 관리 (Memory Management)

### [ ] 고성능 글로벌 할당기 도입 (mimalloc / tcmalloc) - [HOLD]
- **상태**: 현재 `MessagePool` 및 `SimplePool` 최적화에 집중. `Multi-Level MessagePool` 도입 후 재검토 예정.
- **이유**: 외부 종속성 추가 대비 이득 불분명. LFH(Low Fragmentation Heap)가 이미 준수한 성능을 보임.

### [x] Fixed-Size Function Wrapper (무할당 람다) - 롤백 및 최적화 완료
- **성과**: 4KB 고정 풀링 시도 결과 성능 하락(48ms -> 181ms) 확인. 현재 Windows LFH(Low Fragmentation Heap)를 활용한 할당 방식으로 회귀하여 안정성 및 캐시 효율 확보.

---

## 2. 네트워크 및 디바이스 IO (Network & IO)

### [x] 송신 큐(Send Queue) 경합성 개선 (Lock-Free) - 완료
- **성과**: `moodycamel::ConcurrentQueue`를 통한 MPSC 락프리 송신 큐가 전역적으로 적용됨. `try_dequeue_bulk`를 통한 일괄 추출 및 배치 전송(Avg Batch 20+)으로 컨텍스트 스위칭 최소화.

### [x] Hybrid Send Strategy (Gather Write vs Linearize) - 완료
- **성과**: `Session::Flush`에서 `Linearize` 버퍼를 사용한 단일 `async_write` 전략이 기본 적용됨. 대형 패킷에 대해서는 수동 Scatter-Gather가 아닌 시스템 성능에 최적화된 복사를 우선 채택.

### [x] 가변형 제로복사 파이프라인 (Adaptive Zero-Copy Pipeline) - 완료 (구조적 분리)
- **달성**: `GatewaySession`(암호화/Linearize)과 `BackendSession`(평문/Scatter-Gather) 클래스 분리로 구현 완료.
- **상세**:
    - **`GatewaySession`**: 암호화가 필수적이므로 `LinearBuffer` 복사 방식 채택 (1-Copy).
    - **`BackendSession`**: 내부 통신용으로 `std::vector<const_buffer>`를 활용한 Scatter-Gather 전송 (Zero-Copy 지향).
- **향후 과제**: `BackendSession`의 실제 Zero-Copy 효율성 검증 (`todo.md` 참조).

---

## 3. 로직 처리 및 게임성 최적화 (Logic & Gameplay)

### [x] Protobuf Parallel Deserialization - 구조적 완성
- **구조**: IO 스레드가 네트워크 수신 및 복사를 완료하면, `Dispatcher`가 전용 스레드풀에서 `IPacketHandler`를 통해 패킷을 처리하는 구조. 사실상 역직렬화와 로직 처리가 IO와 분리됨.

### [x] 최적화된 객체 습득 및 연출 로직 (Server-Driven, Client-Visual) - **NEW**
- **배경**: 멀티유저 환경에서 자석 효과와 습득 판정을 서버 물리와 고정할 경우 동기화 오차가 발생할 수 있음.
- **해결**: 서버는 `MAGNET_RADIUS` 진입 즉시 확정 습득 처리(EXP 지급)를 하고, 클라이언트는 해당 젬의 '사후 날아가기' 연출만 시각적으로 담당.
- **성과**: 서버 로직 부하 감소 및 사용자 체감 지연 시간 제로 달성.

---

## 4. 기타 (Miscellaneous)

### [ ] SIMD 기반 패킷 직렬화 및 검사 - [HOLD]
- **상태**: 현재 프로파일링 상에서 직렬화/검사가 병목이 아님.
- **계획**: 추후 `ProfileResult` 분석 후 필요시 `todo.md`로 이관.
