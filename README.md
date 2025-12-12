# ToyServer2

**ToyServer의 한계를 넘어 처음부터 다시 설계된 프로젝트입니다.**

본 프로젝트는 기존 `ToyServer` 코드베이스의 구조적 한계와 "스파게티 코드" 위험성을 인지하고, 이를 완전히 뜯어고쳐(Rebuilt) 강력하고 확장 가능하며 유지보수가 용이한 고성능 네트워크 서버 프레임워크를 구축하는 것을 목표로 합니다.

> **🚧 상태: 개발 진행 중 (Work In Progress) 🚧**
>
> 현재 활발히 개발이 진행되고 있습니다. 핵심 프레임워크 안정화 작업과 기능 추가가 점진적으로 이루어지고 있습니다.

## 핵심 특징 (Key Features)

*   **진정한 크로스 플랫폼 (True Cross-Platform)**: **Windows** (IOCP)와 **Linux** (Epoll/io_uring) 모두에서 완벽하게 동작하도록 처음부터 설계되었습니다.
*   **Modern C++20**: `std::span`, `concepts`, `smart pointers` 등 최신 C++ 표준을 적극 활용하여 타입 안전성과 성능을 확보했습니다.
*   **고성능 네트워킹 (High-Performance)**:
    *   **Boost.Asio** 기반의 검증된 비동기 처리.
    *   **Zero-Copy** 패킷 처리 아키텍처.
    *   **Lock-Free** 세션 디스패칭 (Direct Pointer Passing 최적화 적용).
*   **모듈화된 프레임워크 (Modular Framework)**:
    *   **의존성 주입 (DI)** 기반 설계로 테스트 용이성 확보.
    *   `Network`, `Dispatcher`, `Logic` 계층 간의 명확한 역할 분리.

## 요구 사항 (Prerequisites)

`ToyServer2`를 빌드하기 위해 다음 환경이 필요합니다:

*   **컴파일러**: C++20 호환 (MSVC v143+, Clang 10+, GCC 10+)
*   **CMake**: 버전 3.20 이상.
*   **Vcpkg**: Microsoft C++ 패키지 매니저 (`boost`, `tcmalloc`, `spdlog` 등 의존성 관리에 필수).

## 빌드 방법 (Build Instructions)

### Windows (Visual Studio)

가장 간편한 설정을 위해 제공된 헬퍼 스크립트 사용을 권장합니다.

1.  **저장소 클론**:
    ```bash
    git clone https://github.com/your-repo/ToyServer2.git
    cd ToyServer2
    ```

2.  **스크립트로 빌드**:
    루트 디렉토리의 `build.bat`을 실행합니다. 이 스크립트는 `vcpkg`를 자동으로 감지하고 CMake 설정 및 빌드 과정을 수행합니다.
    ```cmd
    .\build.bat
    ```

3.  **수동 빌드 (CMake)**:
    CMake를 직접 실행하고 싶다면 다음 명령어를 참고하세요:
    ```cmd
    cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg설치경로]/scripts/buildsystems/vcpkg.cmake
    cmake --build build --config Debug
    ```

### 서버 실행 (Running the Server)

빌드가 성공하면 `build/Debug` (또는 `Release`) 디렉토리에 실행 파일이 생성됩니다.

*   **GameServer**: `./build/Debug/GameServer.exe`
*   **DummyClient**: `./build/Debug/DummyClient.exe` (스트레스 테스트 및 검증용)

## 디렉토리 구조 (Directory Structure)

*   `src/System`: 핵심 프레임워크 컴포넌트 (Network, Thread, Timer, Memory).
*   `src/GameServer`: 메인 게임 서버 애플리케이션 로직.
*   `src/DummyClient`: 스트레스 테스트용 더미 클라이언트 툴.
*   `doc/`: 개발자 문서 및 코딩 컨벤션.

## License

This project is licensed under the **BSD 3-Clause License**.

> **Condition**:
>
> Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. (See the `LICENSE` file for details.)

