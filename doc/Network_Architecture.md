# Network Architecture (네트워크 아키텍처)

이 문서는 ToyServer2 프로젝트의 하이브리드(TCP + UDP) 네트워크 구조와 데이터 전송 전략을 설명합니다.

---

## 1. 아키텍처 개요

ToyServer2는 **TCP(신뢰성)**와 **UDP(실시간성)**의 장점을 결합한 하이브리드 아키텍처를 채택하고 있습니다.

### 1.1 하이브리드 통신 모델
동일한 클라이언트가 서비스 성격에 따라 두 프로토콜을 동시에 사용합니다.
- **TCP (8080 포트)**: 로그인, 아이템 거래, 인벤토리 관리, 채팅 등 절대적인 신뢰성과 순서 보장이 필요한 작업.
- **UDP (8081 포트)**: 캐릭터 이동, 실시간 동기화, 액션 통지 등 레이턴시가 최우선인 작업.

### 1.2 세션 레이어 구조
```
ISession (Interface)
├── Session (Base Class)
│   ├── GatewaySession (TCP + AES/XOR Encryption) - 외부 클라이언트용
│   ├── BackendSession (TCP + Plaintext) - 서버 간 통신용
│   └── UDPSession (UDP / KCP) - 실시간 동기화용
```

---

## 2. UDP & KCP 상세 설계

### 2.1 프로토콜 스택
UDP 상에 자체적인 전송 헤더와 KCP 레이어를 얹어 다중화(Multiplexing)를 지원합니다.
```
+-------------------+-----------------+----------------+
| UDPTransportHeader|  PacketHeader   |    Payload     |
|     (25 bytes)    |    (4 bytes)    |   (variable)   |
+-------------------+-----------------+----------------+
```

#### UDPTransportHeader (25 bytes)
- **Tag (1B)**: `0x00` (Raw UDP), `0x01` (KCP)
- **SessionId (8B)**: 세션 식별자
- **Token (16B)**: NAT Rebinding 대응을 위한 보안 토큰 (uint128_t)

### 2.2 전송 전략 (Routing Policy)
패킷의 성격에 따라 최적의 전송 방식을 선택합니다.

| 분류 | 전송 방식 | 사용 사례 | 특징 |
|------|---------|----------|------|
| **Critical TCP** | `SendPacket` | 로그인, 거래, 인벤토리 | 절대 손실 불가, 순서 보장 (OS 레벨) |
| **Reliable UDP** | `SendReliable` | 스킬 사용, 아이템 획득 | KCP 사용, 신뢰성 보장 + 빠른 재전송 (20ms) |
| **Unreliable UDP** | `SendUnreliable`| 이동, 위치 동기화 | Raw UDP, 최신 상태만 중요, 손실 허용, 최저 지연 |

---

## 3. 핵심 기능 구현 사항

### 3.1 NAT Rebinding (UDP)
네트워크 환경 변화(WiFi <-> LTE)로 클라이언트의 IP:Port가 바뀌어도, 패킷에 포함된 **128비트 UDP Token**을 통해 기존 세션을 즉시 찾아 이동(Rebinding)시킵니다.

### 3.2 세션 풀링 및 생명주기 (Pooling & Lifecycle)
메모리 파편화 및 할당 오버헤드를 줄이기 위해 모든 세션 타입(`Gateway`, `Backend`, `UDP`)은 미리 생성된 풀에서 관리됩니다.

- **SessionFactory 독점권**: 세션의 객체 생성 및 소멸(반납)은 반드시 `SessionFactory`를 통해서만 이루어져야 합니다. 외부에서 `new`/`delete`를 직접 사용하는 것을 엄격히 금지합니다.
- **Acquire (대여)**: `SessionFactory::Create()`를 통해 풀에서 세션을 대여합니다.
- **Release (반납)**: `SessionFactory::Destroy()` 호출 시 `OnRecycle()`을 통해 멤버 변수들을 초기화하고 풀로 안전하게 반납합니다.
- **Safety**: 공유 포인터(`std::shared_ptr`)를 사용하여 세션이 디스패처 큐에 머무는 동안 객체가 소멸되지 않도록 보장합니다.

---

## 4. 데이터 흐름 (Data Flow)

### 4.1 패킷 수신 (Incoming)
1. **UDP Socket** -> `UDPNetworkImpl` 수신.
2. **Header Parsing**: `UDPTransportHeader`에서 세션 ID와 토큰 확인.
3. **Session Lookup**: Endpoint Registry에서 세션 검색 (실패 시 토큰으로 재검색).
4. **Multiplexing**: 태그가 `0x01`이면 KCP 레이어로 전달, `0x00`이면 즉시 디스패처로 전달.

### 4.2 패킷 발신 (Outgoing)
1. **Application** -> `SendReliable` 또는 `SendUnreliable` 호출.
2. **Serialization**: 객체를 바이너리로 직렬화.
3. **Queueing**: 각 세션의 네트워크 큐에 삽입.
4. **Flush**: 주기적으로 또는 즉시 수송 헤더를 붙여 커널 소켓으로 전송.

---

## 5. 관리 및 모니터링
- **Heartbeat**: TCP/UDP 공통으로 생존 확인 패킷 송수신.
- **Registry**: `UDPEndpointRegistry`를 통해 활성 세션 정보 및 타임아웃 관리.

---
*최종 업데이트: 2026-02-07 (네트워크 문서 통합 및 최신화)*
