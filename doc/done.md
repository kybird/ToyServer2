# 완료된 작업 (Done)

## Phase 2: Operation Tools ✅
- [x] **Structured Logger** - Context fields, Macros
- [x] **Rate Limiter** - Token Bucket (Header-only)
- [x] **Command Console** - Runtime server control via stdin

## Phase 3: Convenience Features ✅
- [x] **Event Bus** - Publish/Subscribe pattern
- [x] **ByteBuffer** - Serialization utility
- [x] **Connection Pool (DB)** - Database connection management

## Phase 4: Game Server Basics ✅
- [x] **Protocol & Packet Handling** - Protobuf 기반 표준화
- [x] **Authentication** - 자동 등록 로그인 플로우
- [x] **Room Management** - 방 생성, 입장, 퇴장 처리

## Phase 5: Advanced Features ✅
- [x] **SQLite Integration** - Lightweight dev DB
- [x] **Crash Handler** - Minidump generation

---

## 아키텍처 리팩토링 ✅
- [x] `UserDB.h` 인터페이스화 (`IDBConnectionPool` 사용)
- [x] `Factory` 클래스들 `IObjectPool` 적용
- [x] `Main.cpp` 팩토리 메서드 적용
- [x] `LoginController.cpp` 불필요 헤더 제거

## 버그 수정 ✅
- [x] **Rate Limiter 기본값 수정** (2025-12-19)
  - 기본값 `rate=1.0, burst=1.0` → `rate=100.0, burst=200.0`
  - 로비 입장 플로우 버그 해결
- [x] **워커 스레드 설정 키 호환성** (2025-12-19)
  - `worker_threads` / `workerThreadCount` 둘 다 지원
- [x] **디버그 로그 정리** (2025-12-19)
  - Hot path 상세 로그 제거, 오류 로그만 유지
- [x] **RateLimiter 동적 설정 적용** (2025-12-20)
  - `JsonConfigLoader` → `Framework` → `SessionFactory` → `Session::Reset` 체인 완성
  - `server_config.json`에 `rate_limit`, `rate_burst` 명시
