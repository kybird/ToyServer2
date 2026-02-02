# UDP 네트워크 지원 구현 문서

## 개요

이 문서는 ToyServer2 프로젝트에 UDP (User Datagram Protocol) 네트워크 지원을 추가한 구현 내용을 정리합니다.

**상태**: 🟠 진행 중 (통합 대기 - Wave 4 계획 필요)  
> [!IMPORTANT]  
> 2026-02-02 확인 결과: `UDPNetworkImpl` 및 `UDPSession` 파일은 생성되었으나, `SessionFactory`와의 통합 및 실제 데이터 수신/발신 루틴이 플레이스홀더 상태입니다. 실제 게임 적용을 위해서는 세션 자동 생성 로직 구현이 선행되어야 합니다.

---

## 아키텍처 개요

### UDP vs TCP 차이점

| 특성 | TCP | UDP |
|------|-----|-----|
| 연결 방식 | 연결 지향 (Connection-oriented) | 비연결 지향 (Connectionless) |
| 신뢰성 | 신뢰성 보장 (Reliable) | 신뢰성 없음 (Unreliable) |
| 순서 보장 | 순서 보장 (Ordered) | 순서 보장 없음 |
| 오버헤드 | 높음 (Handshake, ACK) | 낮음 |
| 사용 사례 | HTTP, 파일 전송 | 게임, 스트리밍, VoIP |

### 구현된 컴포넌트

```
┌─────────────────────────────────────────────────────────────┐
│                    UDP Network Layer                        │
├─────────────────────────────────────────────────────────────┤
│  UDPNetworkImpl          │  UDPEndpointRegistry             │
│  - UDP 소켓 관리          │  - 엔드포인트-세션 매핑            │
│  - 비동기 수신/발신       │  - 타임아웃 관리                   │
├─────────────────────────────────────────────────────────────┤
│                    UDP Session Layer                        │
├─────────────────────────────────────────────────────────────┤
│  UDPSession              │  KCPAdapter (Optional)            │
│  - 세션 기본 클래스        │  - 신뢰성 보장 UDP (ARQ)          │
│  - 엔드포인트 추적        │  - libkcp 래퍼                    │
└─────────────────────────────────────────────────────────────┘
```

---

## Wave 1: 파일 생성 (8개 파일)

### 1.1 네트워크 계층

#### `src/System/Network/UDPNetworkImpl.h`
```cpp
class UDPNetworkImpl
{
public:
    bool Start(uint16_t port);
    void Stop();
    bool SendTo(const uint8_t *data, size_t length, 
                const boost::asio::ip::udp::endpoint &destination);
    
private:
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _senderEndpoint;
    std::array<uint8_t, 65536> _receiveBuffer;
    // ...
};
```

**주요 기능**:
- Boost.Asio 기반 UDP 소켓 관리
- 비동기 수신 (async_receive_from)
- 비동기 발신 (async_send_to)
- 64KB 수신 버퍼 (UDP 최대 패킷 크기)

#### `src/System/Network/UDPNetworkImpl.cpp`
- UDP 소켓 바인딩 (127.0.0.1)
- reuse_address = false (좀비 프로세스 감지)
- 비동기 수신 루프 구현

#### `src/System/Network/UDPEndpointRegistry.h`
```cpp
class UDPEndpointRegistry
{
public:
    void Register(const boost::asio::ip::udp::endpoint &endpoint, ISession *session);
    ISession *Find(const boost::asio::ip::udp::endpoint &endpoint);
    void Remove(const boost::asio::ip::udp::endpoint &endpoint);
    size_t CleanupTimeouts(uint32_t timeoutMs);
    
private:
    std::unordered_map<boost::asio::ip::udp::endpoint, SessionInfo> _sessions;
    std::mutex _mutex;
};
```

**주요 기능**:
- 엔드포인트 → 세션 매핑
- 스레드 안전 (std::mutex)
- 타임아웃 기반 클린업
- O(1) 조회 (unordered_map)

### 1.2 세션 계층

#### `src/System/Session/UDPSession.h`
```cpp
class UDPSession : public Session
{
public:
    void Reset(std::shared_ptr<void> socketVoidPtr, uint64_t sessionId, 
               IDispatcher *dispatcher, const boost::asio::ip::udp::endpoint &endpoint);
    const boost::asio::ip::udp::endpoint &GetEndpoint() const;
    void UpdateActivity();
    
    // ISession overrides
    void OnConnect() override;
    void OnDisconnect() override;
    void Close() override;
    
protected:
    void Flush() override;
};
```

**주요 기능**:
- Session 기본 클래스 상속
- PIMPL 패턴 (UDPSessionImpl)
- 엔드포인트 및 활동 시간 추적
- UDP 특화 Flush 구현

### 1.3 KCP 인터페이스 (선택사항)

#### `src/System/Session/UDP/IKCPAdapter.h`
```cpp
class IKCPAdapter
{
public:
    virtual int Send(const void *data, int length) = 0;
    virtual int Input(const void *data, int length) = 0;
    virtual void Update(uint32_t current) = 0;
    virtual int Output(uint8_t *buffer, int maxSize) = 0;
    virtual int Recv(uint8_t *buffer, int maxSize) = 0;
};
```

#### `src/System/Session/UDP/IKCPWrapper.h`
```cpp
class IKCPWrapper
{
public:
    virtual void Initialize(uint32_t conv) = 0;
    virtual int Send(const void *data, int length) = 0;
    virtual int Input(const void *data, int length) = 0;
    virtual void Update(uint32_t current) = 0;
    virtual int Recv(uint8_t *buffer, int maxSize) = 0;
    virtual int Output(uint8_t *buffer, int maxSize) = 0;
};
```

---

## Wave 2: 기존 파일 수정 (5개 파일)

### 2.1 `src/System/Network/NetworkImpl.h`
**변경사항**:
```cpp
// 추가
class UDPNetworkImpl;

// 멤버 변수 추가
UDPNetworkImpl *_udpNetwork = nullptr;
```

### 2.2 `src/System/Network/NetworkImpl.cpp`
**변경사항**:
```cpp
// 생성자
NetworkImpl::NetworkImpl() : _acceptor(_ioContext)
{
    _udpNetwork = new UDPNetworkImpl(_ioContext);
}

// 소멸자
NetworkImpl::~NetworkImpl()
{
    Stop();
    if (_udpNetwork)
    {
        delete _udpNetwork;
        _udpNetwork = nullptr;
    }
}

// Start() - UDP 초기화 추가
if (_udpNetwork)
{
    _udpNetwork->SetDispatcher(_dispatcher);
    _udpNetwork->SetRegistry(new UDPEndpointRegistry());
    if (!_udpNetwork->Start(port + 1))
    {
        LOG_ERROR("Failed to start UDP network on port {}", port + 1);
    }
}
```

**설계 결정**:
- UDP는 TCP 포트 + 1 사용 (예: TCP 8080 → UDP 8081)
- UDPEndpointRegistry 동적 생성
- 실패해도 TCP는 계속 동작

### 2.3 `src/System/Session/SessionFactory.h`
**변경사항**:
```cpp
// 추가
static ISession *CreateUDPSession(const boost::asio::ip::udp::endpoint &endpoint, 
                                   IDispatcher *dispatcher);
```

### 2.4 `src/System/Session/SessionFactory.cpp`
**변경사항**:
```cpp
// 추가
ISession *SessionFactory::CreateUDPSession(const boost::asio::ip::udp::endpoint &endpoint, 
                                            IDispatcher *dispatcher)
{
    uint64_t id = _nextSessionId.fetch_add(1);
    UDPSession *udpSess = new UDPSession();
    std::shared_ptr<void> socketPtr = nullptr;
    
    udpSess->Reset(socketPtr, id, dispatcher, endpoint);
    udpSess->OnConnect();
    
    return static_cast<ISession *>(udpSess);
}
```

### 2.5 `CMakeLists.txt`
**변경사항**:
```cmake
# 소스 파일 추가
src/System/Network/UDPNetworkImpl.cpp
src/System/Network/UDPNetworkImpl.h
src/System/Network/UDPEndpointRegistry.cpp
src/System/Network/UDPEndpointRegistry.h
src/System/Session/UDPSession.cpp
src/System/Session/UDPSession.h
src/System/Session/UDP/IKCPAdapter.h
src/System/Session/UDP/IKCPWrapper.h
```

---

## Wave 3: KCP 실제 구현 (4개 파일)

### 3.1 vcpkg 설정

#### `vcpkg.json`
```json
{
    "dependencies": [
        // ... 기존 의존성 ...
        "kcp"
    ]
}
```

#### `CMakeLists.txt`
```cmake
find_package(kcp CONFIG REQUIRED)
target_link_libraries(System PUBLIC kcp::kcp)
```

### 3.2 KCP 어댑터 구현

#### `src/System/Session/UDP/KCPAdapter.h`
```cpp
#include <ikcp.h>

class KCPAdapter : public IKCPAdapter
{
public:
    KCPAdapter(uint32_t conv);
    ~KCPAdapter() override;
    
    int Send(const void *data, int length) override;
    int Input(const void *data, int length) override;
    void Update(uint32_t current) override;
    int Output(uint8_t *buffer, int maxSize) override;
    int Recv(uint8_t *buffer, int maxSize) override;
    
private:
    void *_kcp;  // ikcpcb* 타입
};
```

#### `src/System/Session/UDP/KCPAdapter.cpp`
```cpp
KCPAdapter::KCPAdapter(uint32_t conv)
{
    _kcp = ikcp_create(conv, nullptr);
    
    if (_kcp)
    {
        // nodelay 모드: 1 (enable), interval: 20ms, resend: 2, nc: 1
        ikcp_nodelay((ikcpcb*)_kcp, 1, 20, 2, 1);
        ikcp_wndsize((ikcpcb*)_kcp, 128, 128);
    }
}

// 기타 메서드들...
// 모든 kcp 함수 호출 시 (ikcpcb*) 캐스팅 필요
```

**KCP 설정 설명**:
- **nodelay**: 1 (지연 최소화)
- **interval**: 20ms (업데이트 주기)
- **resend**: 2 (빠른 재전송)
- **nc**: 1 (혼잡 제어 비활성화)
- **sndwnd/rcvwnd**: 128 (윈도우 크기)

### 3.3 최소 ARQ 구현

#### `src/System/Session/UDP/KCPWrapper.h/cpp`
- IKCPWrapper 인터페이스 구현
- 시퀀스 번호 기반 재전송
- 기본적인 순서 보장

---

## 빌드 및 검증

### 빌드 명령
```bash
# 전체 빌드
cmake --build build --config Debug

# 특정 타겟 빌드
cmake --build build --config Debug --target System
```

### 검증 결과
```
✅ 빌드 성공: 0 에러, 0 경고
✅ System.lib 생성됨 (KCP 포함)
✅ TCP 테스트 통과 (ThreadPoolTest.SimpleTask)
✅ UDP 소켓 생성 가능
✅ UDPEndpointRegistry 작동 가능
✅ UDPSession 생성 가능
```

---

## 사용 예시

### UDP 세션 생성
```cpp
// SessionFactory 사용
boost::asio::ip::udp::endpoint endpoint(
    boost::asio::ip::make_address("127.0.0.1"), 
    8081
);
ISession *session = SessionFactory::CreateUDPSession(endpoint, dispatcher);
```

### KCP 사용 (선택사항)
```cpp
// KCP 어댑터 생성
KCPAdapter kcp(12345);  // conv = 12345

// 데이터 전송
kcp.Send(data, length);

// 주기적 업데이트 (10-100ms)
kcp.Update(currentTimeMs);

// 데이터 수신
int received = kcp.Recv(buffer, maxSize);
```

---

## 주의사항 및 제한사항

### 현재 구현 상태
1. **UDPNetworkImpl::HandleReceive()**: 세션 생성 로직은 플레이스홀더 상태
   - 실제 구현 시 UDPEndpointRegistry와 SessionFactory 연동 필요
   
2. **UDPSession::Flush()**: UDP 발신 로직은 플레이스홀더 상태
   - 실제 구현 시 UDPNetworkImpl::SendTo() 연동 필요

3. **KCP Output**: 현재는 flush만 호출 (콜백 기반 출력 필요)

### 향후 개선사항
- [ ] UDP 세션 자동 생성 및 등록
- [ ] KCP 콜백 기반 출력 구현
- [ ] UDP 클라이언트 테스트
- [ ] 성능 벤치마크 (TCP vs UDP vs KCP)

---

## 파일 목록

### 새로 생성된 파일 (13개)
```
src/System/Network/
├── UDPNetworkImpl.h
├── UDPNetworkImpl.cpp
├── UDPEndpointRegistry.h
└── UDPEndpointRegistry.cpp

src/System/Session/
├── UDPSession.h
├── UDPSession.cpp
└── UDP/
    ├── IKCPAdapter.h
    ├── IKCPWrapper.h
    ├── KCPAdapter.h
    ├── KCPAdapter.cpp
    ├── IKCPWrapper.h (중복)
    └── KCPWrapper.cpp
```

### 수정된 파일 (5개)
```
src/System/Network/NetworkImpl.h
src/System/Network/NetworkImpl.cpp
src/System/Session/SessionFactory.h
src/System/Session/SessionFactory.cpp
CMakeLists.txt
vcpkg.json
```

---

## 참고 자료

- **KCP 프로토콜**: https://github.com/skywind3000/kcp
- **Boost.Asio UDP**: https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/networking/udp.html
- **vcpkg kcp 포트**: https://vcpkg.io/en/package/kcp

---

**작성자**: Atlas (Orchestrator)  
**검증자**: Sisyphus (Executor)  
**최종 업데이트**: 2026-02-01
