# 아키텍처 위반 리포트 (Architectural Violation Report)

## 개요
ToyServer2 프레임워크 설계 원칙에 따라, 모든 어플리케이션(Application) 프로젝트는 `System` 폴더의 루트에 존재하는 **공식 인터페이스(`.h`)**만을 사용하여 프레임워크 기능에 접근해야 합니다. 현재 `VampireSurvivor` 서버 프로젝트에서 발견된 설계 원칙 위반 사례를 아래와 같이 정리합니다.

## 위반 사례 리스트

### 1. 데이터베이스 접근 (Database Access)
*   **파일**: `src/Examples/VampireSurvivor/Server/Core/UserDB.h`
*   **위반 사항**: `#include "System/Database/DBConnectionPool.h"` 및 `System::DBConnectionPool` 클래스 직접 의존.
*   **해결 방안**: 루트에 노출된 `System/IDBConnectionPool.h` 인터페이스 사용하도록 변경.

### 2. 패킷 구조 및 핸들링 (Packet Message & Handling)
*   **파일**: 
    - `src/Examples/VampireSurvivor/Server/Core/GamePacketHandler.h/cpp`
    - `src/Examples/VampireSurvivor/Server/Game/Room.h/cpp`
*   **위반 사항**: `#include "System/Dispatcher/IMessage.h"` 참조 및 `PacketMessage` 구조체 내부 멤버(`Payload()`, `type` 등) 직접 접근.
*   **해결 방안**: `PacketMessage` 내부 구조를 감추고, 인터페이스 기반의 데이터 접근 방식(예: `PacketUtils`를 통한 페이로드 획득) 도입 검토.

### 3. 유틸리티 및 네트워크 도구 (Network Utilities)
*   **파일**: `src/Examples/VampireSurvivor/Server/Game/Room.h/cpp`
*   **위반 사항**: `#include "System/Network/PacketUtils.h"` 참조.
*   **해결 방안**: `PacketUtils`를 루트로 이동하거나 인터페이스화하여 어플리케이션에서 안전하게 사용하도록 노출.

### 4. 객체 풀링 (Object Pooling)
*   **파일**: 
    - `src/Examples/VampireSurvivor/Server/Entity/MonsterFactory.h`
    - `src/Examples/VampireSurvivor/Server/Entity/PlayerFactory.h`
    - `src/Examples/VampireSurvivor/Server/Entity/ProjectileFactory.h`
*   **위반 사항**: `#include "System/Memory/SimplePool.h"` 참조 및 `SimplePool` 구현체 직접 사용.
*   **해결 방안**: 루트의 `IObjectPool.h` 인터페이스를 사용하도록 변경.

### 5. 세션 관리 (Session Management)
*   **파일**: `src/Examples/VampireSurvivor/Server/Core/LoginController.cpp`
*   **위반 사항**: `#include "System/Session/Session.h"` 참조.
*   **해결 방안**: `ISession.h` 인터페이스만을 사용하도록 로직 수정.

---

## 리팩토링 진행 상황
- [x] `UserDB.h` 인터페이스화 완료
- [x] `Factory` 클래스들 `IObjectPool` 적용 완료
- [ ] `PacketMessage` 직접 접근 로직 제거 및 캡슐화 보호 (검토 중)
- [x] `Main.cpp` 내 팩토리 메서드 적용 (완료)
- [x] `LoginController.cpp` 내 불필요 헤더 제거 (완료)
