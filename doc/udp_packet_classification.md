# UDP 패킷 분류 및 Reliable UDP 사용 전략

## 핵심 원칙: "패킷 특성에 따라 프로토콜 선택"

---

## 패킷 분류

### 1. Unreliable UDP (순수 UDP) - 이동 패킷
```cpp
// 특성: 최신 상태만 중요, 손실 허용
SendPacket_UDP_Unreliable(MOVE_UPDATE, position);
```

**사용 사례**:
- ✅ **캐릭터 이동** (가장 최신 위치만 중요)
- ✅ **몬스터 위치 동기화**
- ✅ **카메라 회전**
- ✅ **애니메이션 상태**

**이유**:
```
시간 T0: 위치 (10, 20) 전송 → 패킷 손실!
시간 T1: 위치 (11, 21) 전송 → 도착
시간 T2: 위치 (12, 22) 전송 → 도착

결과: T0 패킷 손실해도 문제없음 (T1이 더 최신)
재전송 불필요! (오히려 대역폭 낭비)
```

---

### 2. Reliable UDP (KCP) - 중요한 액션
```cpp
// 특성: 신뢰성 필요하지만 레이턴시도 중요
SendPacket_KCP(SKILL_CAST, skillId);
```

**사용 사례**:
- ✅ **스킬 사용** (반드시 도착해야 함)
- ✅ **아이템 획득**
- ✅ **몬스터 공격**
- ✅ **버프/디버프 적용**

**이유**:
```
스킬 사용 패킷 손실 시:
- 클라이언트: 스킬 사용함
- 서버: 스킬 안 받음
→ 동기화 깨짐!

KCP 사용:
- 빠른 재전송 (20ms 간격)
- TCP보다 낮은 레이턴시
- 신뢰성 보장
```

---

### 3. TCP - 절대 손실 불가
```cpp
// 특성: 순서 보장 + 신뢰성 필수
SendPacket_TCP(TRADE_CONFIRM, itemId, gold);
```

**사용 사례**:
- ✅ **로그인/인증**
- ✅ **아이템 거래**
- ✅ **인벤토리 변경**
- ✅ **채팅 메시지**
- ✅ **퀘스트 완료**

**이유**:
```
거래 시나리오:
1. 골드 차감 (패킷 A)
2. 아이템 지급 (패킷 B)

순서 보장 필수!
B가 A보다 먼저 도착하면 복사 버그
```

---

## 실제 게임 예시: 액션 RPG

### 시나리오 1: 전투 중 이동
```cpp
// 60 FPS 게임, 초당 60번 위치 업데이트
void OnPlayerMove(Vector3 position)
{
    // ❌ 잘못된 방법: Reliable UDP
    SendPacket_KCP(MOVE_UPDATE, position);
    // 문제: 재전송으로 대역폭 폭증 (60 * 재전송)
    
    // ✅ 올바른 방법: Unreliable UDP
    SendPacket_UDP(MOVE_UPDATE, position);
    // 1-2개 패킷 손실해도 다음 프레임에 보정됨
}
```

### 시나리오 2: 스킬 사용
```cpp
void OnSkillCast(uint32_t skillId)
{
    // ✅ Reliable UDP 사용
    SendPacket_KCP(SKILL_CAST, skillId);
    // 이유:
    // - 반드시 도착해야 함 (쿨다운, 마나 소모)
    // - 레이턴시 중요 (빠른 반응)
    // - 빈도 낮음 (초당 1-2회)
}
```

### 시나리오 3: 아이템 획득
```cpp
void OnItemPickup(uint32_t itemId)
{
    // ⚠️ 선택지 1: Reliable UDP (권장)
    SendPacket_KCP(ITEM_PICKUP, itemId);
    // 장점: 빠른 피드백 (50-100ms)
    
    // ⚠️ 선택지 2: TCP
    SendPacket_TCP(ITEM_PICKUP, itemId);
    // 장점: 더 높은 신뢰성
    // 단점: 느린 피드백 (100-200ms)
    
    // 결정 기준: 게임 장르
    // - 액션 게임 → KCP
    // - MMORPG → TCP
}
```

---

## ToyServer2의 UDP 구현 전략

### 현재 구현 (문서 기준)

```cpp
// UDPSession.h
class UDPSession : public Session
{
    // 기본 UDP 소켓 (Unreliable)
    void Flush() override;
};

// 선택적 KCP 어댑터
class IKCPAdapter
{
    int Send(const void *data, int length);  // Reliable
};
```

### 권장 아키텍처

```cpp
class UDPSession : public Session
{
public:
    // Unreliable UDP (이동, 위치)
    void SendUnreliable(const IPacket &pkt);
    
    // Reliable UDP (스킬, 아이템)
    void SendReliable(const IPacket &pkt);
    
private:
    UDPNetworkImpl *_network;      // Raw UDP
    IKCPAdapter *_kcpAdapter;      // Optional KCP
};
```

### 사용 예시

```cpp
// 게임 로직
void GameSession::OnPlayerMove(Vector3 pos)
{
    MovePacket pkt;
    pkt.position = pos;
    
    // Unreliable UDP
    _udpSession->SendUnreliable(pkt);
}

void GameSession::OnSkillCast(uint32_t skillId)
{
    SkillPacket pkt;
    pkt.skillId = skillId;
    
    // Reliable UDP (KCP)
    _udpSession->SendReliable(pkt);
}

void GameSession::OnTrade(uint32_t itemId, uint32_t gold)
{
    TradePacket pkt;
    pkt.itemId = itemId;
    pkt.gold = gold;
    
    // TCP (절대 손실 불가)
    _tcpSession->SendPacket(pkt);
}
```

---

## 대역폭 비교

### 시나리오: 10명 플레이어, 60 FPS

#### Unreliable UDP (이동)
```
패킷 크기: 20 bytes (위치 + 방향)
초당 전송: 60 packets/sec
대역폭: 20 * 60 * 10 = 12 KB/sec
손실률 1% → 재전송 없음
```

#### Reliable UDP (이동) - ❌ 비효율
```
패킷 크기: 20 bytes + 8 bytes (KCP 헤더)
초당 전송: 60 packets/sec
손실률 1% → 재전송 0.6 packets/sec
대역폭: 28 * 60 * 10 + 재전송 = 17+ KB/sec
오버헤드: +40%
```

#### TCP (이동) - ❌ 최악
```
패킷 크기: 20 bytes + 40 bytes (TCP/IP 헤더)
초당 전송: 60 packets/sec
HOL Blocking: 1개 손실 시 모든 후속 패킷 대기
레이턴시: 100-300ms (재전송 대기)
```

---

## 결론 및 권장사항

### 패킷 분류 기준

| 패킷 타입 | 프로토콜 | 빈도 | 손실 허용 | 레이턴시 |
|----------|---------|------|----------|---------|
| 이동/위치 | UDP | 높음 (60/s) | ✅ | 낮음 |
| 스킬/액션 | KCP | 중간 (1-5/s) | ❌ | 낮음 |
| 거래/인벤 | TCP | 낮음 (0.1/s) | ❌ | 중간 |
| 채팅 | TCP | 낮음 | ❌ | 높음 OK |

### ToyServer2 구현 우선순위

1. **Phase 1**: Unreliable UDP (이동 패킷)
   - 가장 큰 성능 향상
   - 구현 단순

2. **Phase 2**: Reliable UDP (스킬 패킷)
   - KCP 통합
   - 선택적 신뢰성

3. **Phase 3**: 하이브리드 최적화
   - 패킷 타입별 자동 라우팅
   - 동적 프로토콜 전환

### 답변 요약

**Q: Reliable UDP로 이동 패킷을 구현하는가?**

**A: 아니요! 이동 패킷은 Unreliable UDP가 최적입니다.**

- 이동: Unreliable UDP (최신 상태만 중요)
- 스킬: Reliable UDP/KCP (신뢰성 + 낮은 레이턴시)
- 거래: TCP (절대 손실 불가 + 순서 보장)

**Reliable UDP는 "중요하지만 빠른" 패킷용입니다.**
