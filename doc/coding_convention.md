# 코딩 컨벤션 (Coding Conventions)

이 문서는 ToyServer2 프로젝트에서 사용되는 코딩 표준과 아키텍처 패턴을 설명합니다. 일관성과 모듈성을 유지하기 위해 이 가이드라인을 준수해 주십시오.

## 1. 디렉토리 구조 및 가시성 (Directory Structure & Visibility)

### **인터페이스 (Public API)**
*   **위치**: 모듈의 루트 디렉토리.
*   **네이밍**: `I` 접두어 + PascalCase (예: `IDispatcher.h`, `ISession.h`).
*   **목적**: 계약(Contract)을 정의합니다. 다른 모듈에서 포함(include)할 수 있는 **유일한** 파일들입니다.

### **구현 (Private Details)**
*   **위치**: 모듈 내 하위 디렉토리.
*   **네이밍**:
    *   디렉토리: 보통 대문자 또는 기술명 사용 (예: `DISPATCHER`, `ASIO`, `BOOTSTRAP`).
    *   파일/클래스: `Impl` 접미사 또는 구체적인 기술명 사용 (예: `DispatcherImpl.h`, `AsioSession.h`).
*   **목적**: 구체적인 로직. 이 파일들은 가능한 한 외부 모듈에서 직접 포함해서는 **안 됩니다**. 의존성 주입(DI)을 사용하십시오.

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
