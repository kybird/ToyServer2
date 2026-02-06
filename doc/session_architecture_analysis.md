# SessionFactory 아키텍처 분석

## 현재 상황 이해

### 세션 타입 구조

```
ISession (인터페이스)
├── Session (기본 클래스)
│   ├── GatewaySession (TCP + 암호화)
│   ├── BackendSession (TCP + 평문)
│   └── UDPSession (UDP + KCP 옵션)
```

### 핵심 질문들

#### Q1: Gateway vs Backend는 암호화 차이만?
**A: 맞습니다!**

```cpp
// Gateway: 외부 클라이언트 (인터넷)
- TCP 연결
- 패킷 암호화 (AES/XOR)
- Rate Limiting
- Heartbeat

// Backend: 내부 서버 간 통신
- TCP 연결
- 평문 (Zero-Copy 최적화)
- 암호화 오버헤드 없음
- 신뢰할 수 있는 네트워크 가정
```

**설계 의도**: 
- Gateway/Backend 구분은 **보안 정책**의 차이
- 프로토콜(TCP)은 동일

---

#### Q2: TCP와 UDP를 동시 지원하는 경우는?
**A: 있습니다! 하이브리드 아키텍처**

### 게임 서버의 일반적인 패턴:

#### 패턴 1: TCP Only (전통적)
```
클라이언트 ←→ [TCP] ←→ 게임 서버
```
- 장점: 구현 단순, 신뢰성 보장
- 단점: 레이턴시 높음 (Head-of-Line Blocking)

#### 패턴 2: UDP Only + Reliable UDP (KCP/ENet)
```
클라이언트 ←→ [UDP + KCP] ←→ 게임 서버
```
- 장점: 레이턴시 낮음, 패킷 손실 허용
- 단점: 구현 복잡, 방화벽 이슈

#### 패턴 3: 하이브리드 (ToyServer2의 목표!)
```
클라이언트 ←→ [TCP] ←→ 게임 서버  (로그인, 채팅, 거래)
           ←→ [UDP] ←→              (이동, 전투, 실시간 액션)
```

**사용 사례**:
- **TCP**: 로그인, 인벤토리, 채팅, 거래 (신뢰성 필수)
- **UDP**: 캐릭터 이동, 스킬 사용, 몬스터 위치 (실시간성 우선)

---

#### Q3: Reliable UDP를 구현했으면 TCP는 불필요?
**A: 아닙니다! 각자 역할이 다릅니다**

### TCP vs Reliable UDP (KCP) 비교

| 특성 | TCP | KCP (Reliable UDP) |
|------|-----|-------------------|
| **신뢰성** | OS 레벨 보장 | 애플리케이션 레벨 |
| **순서 보장** | 엄격함 (HOL Blocking) | 선택적 (부분 순서) |
| **레이턴시** | 높음 (재전송 대기) | 낮음 (빠른 재전송) |
| **방화벽** | 통과 쉬움 | 차단될 수 있음 |
| **NAT 통과** | 쉬움 | 복잡함 |
| **구현 복잡도** | 낮음 (OS 제공) | 높음 (직접 구현) |

### 실제 게임 시나리오

```cpp
// 시나리오 1: 아이템 거래 (TCP 필수)
// - 패킷 손실 시 돈/아이템 복사 버그 가능
// - 순서 보장 필수 (차감 → 지급)
SendPacket_TCP(TRADE_REQUEST);
SendPacket_TCP(TRADE_CONFIRM);

// 시나리오 2: 캐릭터 이동 (UDP 적합)
// - 최신 위치만 중요 (중간 패킷 손실 OK)
// - 레이턴시 최소화
SendPacket_UDP(MOVE_UPDATE, position);

// 시나리오 3: 스킬 사용 (Reliable UDP)
// - 신뢰성 필요하지만 레이턴시도 중요
// - KCP로 빠른 재전송
SendPacket_KCP(SKILL_CAST, skillId);
```

---

## ToyServer2의 설계 의도

### 문서에서 확인된 내용:

```cpp
// UDP_Network_Implementation.md
"UDP는 TCP 포트 + 1 사용 (예: TCP 8080 → UDP 8081)"
```

**의미**: 
- 동일 클라이언트가 TCP와 UDP를 **동시에** 사용
- 세션 ID로 연결 (TCP 세션 123 = UDP 세션 123)

### 예상 사용 패턴:

```cpp
// 클라이언트 연결 시퀀스
1. TCP 연결 (8080) → 로그인, 인증
2. UDP 토큰 발급 → 클라이언트에게 전달
3. UDP 연결 (8081) → 토큰으로 세션 매핑
4. 이후 TCP/UDP 병행 사용

// 서버 측
GatewaySession *tcpSession = ...;  // 로그인, 채팅
UDPSession *udpSession = ...;      // 이동, 전투
// 동일 플레이어, 다른 프로토콜
```

---

## SessionFactory::Destroy() 문제 재분석

### 현재 코드의 문제:

```cpp
void SessionFactory::Destroy(ISession *session)
{
    if (_serverRole == ServerRole::Gateway)
        pool.Release(static_cast<GatewaySession *>(session));
    else
        pool.Release(static_cast<BackendSession *>(session));
    
    // ❌ UDPSession은 어디로?
}
```

### 문제의 본질:

**`_serverRole`은 TCP 세션의 보안 정책이지, 프로토콜 구분이 아님!**

```cpp
// 잘못된 가정
_serverRole == Gateway  → GatewaySession
_serverRole == Backend  → BackendSession

// 실제 상황
_serverRole == Gateway  → GatewaySession (TCP) + UDPSession (UDP)
_serverRole == Backend  → BackendSession (TCP) + UDPSession (UDP)
```

---

## 올바른 해결 방법

### Option 1: 프로토콜 타입 추가 (권장)

```cpp
// ISession.h
enum class SessionProtocol {
    TCP,
    UDP
};

class ISession {
    virtual SessionProtocol GetProtocol() const = 0;
};

// SessionFactory.cpp
void SessionFactory::Destroy(ISession *session)
{
    if (!session)
        return;

    if (session->GetProtocol() == SessionProtocol::UDP)
    {
        // UDP는 풀링 안 함 (현재)
        delete static_cast<UDPSession *>(session);
        return;
    }

    // TCP 세션은 기존 로직
    if (_serverRole == ServerRole::Gateway)
        GetSessionPool<GatewaySession>().Release(...);
    else
        GetSessionPool<BackendSession>().Release(...);
}
```

### Option 2: dynamic_cast 사용 (안전하지만 느림)

```cpp
void SessionFactory::Destroy(ISession *session)
{
    if (!session)
        return;

    if (auto *udp = dynamic_cast<UDPSession *>(session))
    {
        delete udp;
        return;
    }

    // TCP 세션 처리...
}
```

### Option 3: UDP 세션도 풀링 (장기적)

```cpp
void SessionFactory::Destroy(ISession *session)
{
    if (!session)
        return;

    if (session->GetProtocol() == SessionProtocol::UDP)
    {
        GetSessionPool<UDPSession>().Release(static_cast<UDPSession *>(session));
        return;
    }

    // TCP 세션 처리...
}
```

---

## 최종 권장사항

### 즉시 수정 (Option 1):

1. **ISession에 GetProtocol() 추가**
2. **Destroy()에서 프로토콜 구분**
3. **UDP는 delete로 직접 해제** (풀링 나중에)

### 이유:
- ✅ 안전함 (메모리 누수 해결)
- ✅ 명확함 (프로토콜 vs 보안 정책 분리)
- ✅ 확장 가능 (향후 WebSocket 등 추가 가능)
- ✅ 성능 영향 없음 (가상 함수 1회 호출)

### 장기 계획:
- UDP 세션 풀링 구현
- TCP/UDP 세션 연결 (동일 플레이어)
- 프로토콜별 통계 수집

---

## 결론

**Q: TCP와 UDP를 동시 지원하는 경우는?**  
**A: 바로 ToyServer2의 목표입니다!**

- 로그인/거래 → TCP (신뢰성)
- 이동/전투 → UDP (실시간성)
- 하이브리드 아키텍처로 최적의 성능

**현재 버그**: `Destroy()`가 UDP를 고려하지 않음  
**해결책**: 프로토콜 타입 구분 추가
