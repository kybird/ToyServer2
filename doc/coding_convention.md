# 코딩 컨벤션 (Coding Conventions)

이 문서는 ToyServer2 프로젝트에서 사용되는 코딩 표준과 아키텍처 패턴을 설명합니다. 일관성과 모듈성을 유지하기 위해 이 가이드라인을 준수해 주십시오.

## 1. 디렉토리 구조 및 가시성 (Directory Structure & Visibility)

### **인터페이스 (Public API)**
*   **위치**: 모듈의 루트 디렉토리.
*   **네이밍**: `I` 접두어 + PascalCase (예: `IDispatcher.h`, `ISession.h`).
*   **목적**: 계약(Contract)을 정의합니다. 다른 모듈에서 포함(include)할 수 있는 **유일한** 파일들입니다.
*   **중요**: 향후 Framework는 라이브러리(Static Lib/DLL) 형태로 배포될 예정이며, 오직 이 인터페이스들만 외부로 노출(Export)됩니다. 따라서 내부 구현 클래스에 직접 의존성을 가지는 코드는 빌드 오류를 유발하게 됩니다.

### **구현 (Private Details)**
*   **위치**: 모듈 내 하위 디렉토리.
*   **네이밍**:
    *   디렉토리: 보통 대문자 또는 기술명 사용 (예: `DISPATCHER`, `ASIO`, `BOOTSTRAP`).
    *   파일/클래스: `Impl` 접미사 또는 구체적인 기술명 사용 (예: `DispatcherImpl.h`, `AsioSession.h`).
*   **목적**: 구체적인 로직. 이 파일들은 외부 모듈(Application)에서 직접 포함하는 것이 **절대 금지**됩니다.
*   **의존성 관리**: 항상 `I`로 시작하는 인터페이스를 통해 접근하고, 실제 객체 생성은 Factory 또는 DI를 통해서만 수행하십시오.

**예시:**
```
src/System/Dispatcher/
├── IDispatcher.h          (공개 인터페이스)
└── DISPATCHER/            (숨겨진 구현)
    ├── DispatcherImpl.h
    └── DispatcherImpl.cpp
```

## 2. 네이밍 규칙 (Naming Conventions)

### **클래스 이름**
*   **'Manager' 사용 지양**: `Manager` 접미사 사용을 피하십시오 (예: `PacketManager`, `SessionManager`). 이는 모호하며 종종 God Class를 초래합니다.
*   **선호하는 대안**:
    *   `Service` (예: `AsioService`)
    *   `System`
    *   `Controller`
    *   `Dispatcher`, `Factory`, `Loader` (기능적 이름)
*   **인터페이스**: 항상 `I`로 시작합니다.

### **변수**
*   **멤버 변수**: `_` 접두어 사용 (예: `_dispatcher`, `_socket`).
*   **전역/정적 변수**: 명시적인 접두어(`g_`)를 사용하기도 하지만, DI를 선호합니다.

## 3. 아키텍처 패턴 (Architecture Patterns)

### **의존성 주입 (Dependency Injection, DI)**
*   전역 싱글톤이나 정적 접근자보다는 생성자를 통한 의존성 주입을 선호합니다.
*   예: `AsioSession`은 `GetDispatcher()`를 호출하는 대신 `Reset()`에서 `IDispatcher*`를 주입받습니다.

### **RAII & 소유권**
*   리소스 관리에는 `std::shared_ptr` / `std::unique_ptr`를 사용합니다.
*   `Framework` 클래스는 `AsioService`, `DispatcherImpl` 등 핵심 컴포넌트의 수명(소유권)을 명시적으로 관리합니다.

## 4. 핫패스 가이드라인 (Hot Path Guidelines)

핫패스에서는 **"알림만 하고, 아무것도 하지 마라."**는 대원칙을 준수합니다.

### 4.1. 핫패스 정의 및 구간
다음 구간은 핫패스로 간주하며, "지연 가능성"이 있는 모든 작업이 금지됩니다.
*   IO 스레드 (ASIO poll / epoll loop)
*   패킷 수신/송신 직후 경로
*   타이머 만료 감지 콜백 (ASIO callback)
*   `dispatcher->Post()` 호출 직전까지

### 4.2. 절대 금지 조건 (Hard No)
1.  **Lock / Mutex**: `std::mutex`, `shared_mutex`, `spinlock`, `condition_variable` 등 (이유: 경쟁 시 레이턴시 폭증, IO 스레드 스톨)
2.  **동적 메모리 할당**: `new`, `delete`, `malloc`, `free`, `std::make_shared`, `std::vector::push_back()` 등 (허용 예외: 미리 할당된 풀이나 락프리 프리리스트)
3.  **사용자 코드 실행**: 게임 로직, 리스너 콜백 실행, 가상 함수 체이닝 등 (이유: 실행 시간 예측 불가)
4.  **상태 접근/변경**: 타이머 컨테이너 접근, 맵/벡터 스캔, `weak_ptr.lock()` 등

### 4.3. 허용 조건 (Hot Path OK)
*   단순 계산 (정수 연산, 포인터/핸들 복사)
*   Lock-free Queue에 `push`
*   미리 할당된 풀 객체를 이용한 메시지 포스팅 (`dispatcher->Post(msg)`)

### 4.4. 핫패스 안전 핵심 규칙 (Golden Rules)
*   **Rule 1**: 핫패스는 "사실상 ISR(Interrupt Service Routine)"처럼 취급한다.
*   **Rule 2**: 핫패스에서는 상태를 변경하지 않는다. 상태 변경은 반드시 Logic Thread(Dispatcher 루프 안)에서만 수행한다.
*   **Rule 3**: 핫패스는 "알림만 한다". (Timer expired -> 알림, Packet arrived -> 알림 등)
*   **Rule 4**: 실행 시간은 O(1) + 상수를 유지해야 한다. (루프 금지, 조건 분기 최소화)

### 4.5. 책임 분리
*   **IO / ASIO**: 이벤트 감지 (핫패스)
*   **Dispatcher**: 이벤트 직렬화 및 라우팅 (Router)
*   **Logic Thread**: 실제 게임 로직 및 상태 관리 처리

## 5. 데이터베이스 규칙 (Database Rules)

### 5.1. DbResult<T> 사용 패턴 (소유권 이전 필수)
*   **원칙**: `ITransaction`, `IResultSet` 등 **RAII 객체**를 반환받을 때는 반드시 소유권을 이전받아야 합니다.
*   **방법**: `auto tx = std::move(*res.value);` 처럼 `std::move`를 사용합니다.
*   **이유**: 참조(`auto&`)를 사용하면 RAII 주체가 `res` (임시 객체 또는 결과 컨테이너)에 남게 되어, `res` 소멸 시 의도치 않은 롤백이나 자원 해제가 발생합니다.
*   **예시**:
    ```cpp
    auto res = db->BeginTransaction();
    if (res.status.IsOk()) {
        auto tx = std::move(*res.value); // ✅ 정답: 이제 tx가 유일한 소유자이며 RAII 주체임
        // ... 작업
        tx->Commit();
    } // tx가 스코프를 벗어나며 소멸 (Commit 안 했으면 여기서 자동 Rollback)
    ```

### 5.2. 트랜잭션 관리
*   반드시 RAII(`ITransaction`)를 사용하며, 명시적으로 `Commit()`을 호출하지 않으면 스코프 종료 시 자동 Rollback 됩니다.
## 6. 네트워크 규칙 (Networking Rules)

### 6.1. UDP 전송 방식 선택 (Dual Protocol)
패킷의 성격에 따라 반드시 올바른 전송 메서드를 사용하십시오.
*   **`SendReliable(const IPacket&)`**: 반드시 전달되어야 하는 액션 (스킬 사용, 아이템 획득, 상태 이상 등). KCP를 통해 재전송을 보장합니다.
*   **`SendUnreliable(const IPacket&)`**: 유실되어도 무방하며 최신값만 중요한 데이터 (캐릭터/몬스터 이동, 단순 위치 동기화). Raw UDP를 통해 최저 지연으로 전송합니다.

### 6.2. 세션 수명 주기 관리
*   **생성/소멸**: 세션은 반드시 `SessionFactory`를 통해서만 생성 및 파괴되어야 합니다. 직접적인 `new`/`delete`는 절대 금지됩니다.
*   **풀링(Pooling)**: 모든 세션은 풀링됩니다. 새로운 세션 타입 추가 시 반드시 `OnRecycle()` 메서드를 구현하여, 풀에 반납되기 전 모든 리소스(소켓, 큐, 버퍼, 토큰 등)를 초기화해야 합니다.
*   **안전한 파괴**: 세션 파괴 시에는 `OnDisconnect()` 및 `OnRecycle()` 흐름이 보장되도록 `SessionFactory::Destroy()`를 호출하십시오.

### 6.3. 패킷 직렬화 및 스레드 안전성
*   **참조 캡처 금지**: 비동기 핸들러(람다)에 `IPacket` 참조를 직접 캡처하는 것은 댕글링 포인터 위험이 크므로 **절대 금지**합니다.
*   **직렬화 후 전달**: 반드시 `MessagePool::AllocatePacket()`을 사용하여 패킷 내용을 복사(직렬화)한 후, `PacketPtr`을 값으로 캡처하여 전달하십시오.
*   **Zero-Overhead**: 내부 통신(Backend)이나 고성능이 필요한 경우, `PacketPtr`의 Reference Counting을 활용하여 불필요한 메모리 할당을 최소화하십시오.

### 6.4. 네트워크 핫패스 (Hot Path)
*   UDP 수신 핸들러 (`HandleReceive`) 및 KCP 업데이트 루프는 초고속 핫패스입니다.
*   이 구간에서 **컴파일러 경고가 발생하는 코드, 블로킹 락(Mutex), 복잡한 로직 지연**은 전체 서버 지연으로 직결되므로 엄격하게 관리하십시오.
