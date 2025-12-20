
## [Critical Fix] Lobby Crash & Packet Broadcast Refactoring

### 1. Lobby Dangling Pointer Fix
**증상**: 클라이언트가 로비 입장 후 연결 종료 시, 또는 채팅 시도 시 서버 크래시 발생.
**원인**: `Dispatcher`가 세션 연결을 끊고 메모리를 해제했음에도, `RoomManager::_lobbySessions` 맵에는 해당 세션의 포인터(`ISession*`)가 남아있었음. 이후 브로드캐스트 시도 시 Dangling Pointer 접근으로 인한 Access Violation 발생.
**해결**:
*   `IPacketHandler` 인터페이스에 `OnSessionDisconnect(ISession* session)` 가상 함수 추가.
*   `GamePacketHandler`에서 이를 구현하여, 연결 종료 시 `RoomManager::LeaveLobby` 및 `UnregisterPlayer`를 호출하도록 함.
*   `DispatcherImpl::Process`에서 `NETWORK_DISCONNECT` 처리 시 핸들러의 `OnSessionDisconnect`를 명시적으로 호출.

### 2. Packet Broadcast Refactoring (Type Punning -> Single Ownership)
**증상**: `PacketBroadcast::Broadcast` 호출 중 `PacketStorage::Release`에서 크래시.
**분석**:
*   성능 최적화를 위해 `PacketBuilder`가 `PacketMessage`를 할당한 후 `PacketStorage`로 Type Punning(reinterpret_cast)하여 사용.
*   이 과정에서 `PacketStorage`의 멤버 변수들이 `PacketMessage`의 `vptr` 영역을 덮어씀.
*   `Release` 시 `MessagePool::Free`가 복귀하며 소멸자를 호출하려 할 때 오염된 `vptr`로 인해 크래시 발생.

**시도 및 결정 (Refactoring Journey)**:
1.  **시도 1 (FreeRaw)**: `MessagePool`에 소멸자를 부르지 않는 `FreeRaw` 추가. -> *기각 (임시 미봉책, 위험함)*
2.  **시도 2 (Inheritance)**: `PacketStorage`가 `IMessage`를 상속. -> *기각 (구조 복잡성)*
3.  **시도 3 (RefCounting)**: `PacketMessage`에 `refCount` 추가하여 Shared Ownership 시도. -> *기각 (Atomic False Sharing 및 성능 저하 우려)*
4.  **최종 해결 (Single Ownership & Copy)**:
    *   **Shared State 제거**: `refCount` 및 `PacketStorage` 클래스 완전 삭제.
    *   **Copy Strategy**: `Session::SendPreSerialized` 도입. 원본 메시지(Template)를 받아 **Memcpy**로 각 세션별 전용 사본을 생성하여 전송.
    *   **장점**: 멀티스레드 경합(Contention) 없음, False Sharing 방지, 메모리 수명 관리 명확화.

**결과**:
*   `PacketStorage` 관련 코드 삭제.
*   `Broadcast` 구현이 Safe-Copy 방식으로 변경됨.
*   서버 안정성 확보 및 고성능 원칙(Single Ownership) 준수.
