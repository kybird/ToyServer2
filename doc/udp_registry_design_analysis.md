# UDPEndpointRegistry 설계 분석

## 문제 상황
`_tokens` 맵에서 `_sessions`의 SessionInfo를 어떻게 참조할 것인가?

---

## Option 1: 포인터 저장 (현재 - 버그)

```cpp
std::unordered_map<uint128_t, SessionInfo*> _tokens;
_tokens[udpToken] = &_sessions[endpoint];
```

### 장점
- ✅ 메모리 효율 최고 (포인터 8바이트만 저장)
- ✅ 데이터 일관성 자동 보장 (_sessions 수정 시 _tokens도 자동 반영)

### 단점
- ❌ **치명적 버그**: unordered_map rehashing 시 dangling pointer
- ❌ 런타임 크래시 위험
- ❌ 메모리 손상 가능성

### 평가
**사용 불가** - 안전성 문제로 절대 사용 금지

---

## Option 2: SessionInfo 값 복사 (현재 수정안)

```cpp
std::unordered_map<uint128_t, SessionInfo> _tokens;
_tokens[udpToken] = _sessions[endpoint];  // 값 복사
```

### 장점
- ✅ 안전함 (dangling pointer 없음)
- ✅ 1단계 조회 (O(1))
- ✅ 코드 단순

### 단점
- ❌ **데이터 불일치 문제**:
  ```cpp
  // _sessions의 lastActivity 업데이트
  UpdateActivity(endpoint);  // _sessions[endpoint].lastActivity 갱신
  // ❌ _tokens[token].lastActivity는 여전히 옛날 값!
  ```
- ❌ 메모리 중복 (세션당 24바이트 × 2)
- ❌ **동기화 복잡도 증가**:
  - `UpdateActivity()` 호출 시 _tokens도 업데이트 필요
  - `Register()` 호출 시 _tokens도 업데이트 필요
  - 버그 발생 가능성 높음

### 실제 문제 시나리오
```cpp
// 1. 세션 등록
RegisterWithToken(endpoint, session, token);
// _sessions[endpoint].lastActivity = T0
// _tokens[token].lastActivity = T0

// 2. 패킷 수신 -> UpdateActivity 호출
UpdateActivity(endpoint);
// _sessions[endpoint].lastActivity = T1
// _tokens[token].lastActivity = T0  ❌ 여전히 옛날 값!

// 3. CleanupTimeouts 실행
// _sessions는 T1이라 살아남음
// 하지만 _tokens[token]은 T0이라 타임아웃으로 오판 가능
```

### 평가
**사용 가능하나 위험** - 동기화 로직 추가 필수, 유지보수 부담

---

## Option 3: endpoint 키 저장 (권장)

```cpp
std::unordered_map<uint128_t, boost::asio::ip::udp::endpoint> _tokens;
_tokens[udpToken] = endpoint;  // 키만 저장
```

### 장점
- ✅ 안전함 (dangling pointer 없음)
- ✅ **데이터 일관성 자동 보장** (항상 _sessions를 조회하므로)
- ✅ 메모리 효율 적절 (endpoint 28바이트)
- ✅ 동기화 불필요 (단일 진실 공급원: _sessions)
- ✅ 유지보수 용이

### 단점
- ⚠️ 2단계 조회 (O(1) + O(1) = 여전히 O(1))
  ```cpp
  auto tokenIt = _tokens.find(token);           // 1단계
  auto sessionIt = _sessions.find(tokenIt->second);  // 2단계
  ```
- ⚠️ 코드 약간 길어짐 (하지만 명확함)

### 성능 분석
```
Option 2 (값 복사): 1회 해시 조회
Option 3 (키 저장):  2회 해시 조회

실제 성능 차이:
- 해시 조회 1회: ~10-50ns (L1 캐시 히트 시)
- 추가 조회 비용: ~10-50ns
- 총 차이: 무시할 수준 (마이크로초 이하)

vs

Option 2의 동기화 비용:
- UpdateActivity 호출마다 _tokens도 업데이트
- 매 패킷마다 발생 (초당 수천~수만 회)
- 실제로 Option 3보다 느릴 가능성 높음
```

### 구현 예시
```cpp
// 헤더
std::unordered_map<uint128_t, boost::asio::ip::udp::endpoint> _tokens;

// RegisterWithToken
_tokens[udpToken] = endpoint;

// GetEndpointByToken
ISession *UDPEndpointRegistry::GetEndpointByToken(uint128_t token)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto tokenIt = _tokens.find(token);
    if (tokenIt != _tokens.end())
    {
        auto sessionIt = _sessions.find(tokenIt->second);
        if (sessionIt != _sessions.end())
        {
            return sessionIt->second.session;
        }
    }
    return nullptr;
}

// UpdateActivity - 변경 불필요! (자동 일관성)
void UDPEndpointRegistry::UpdateActivity(const boost::asio::ip::udp::endpoint &endpoint)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(endpoint);
    if (it != _sessions.end())
    {
        it->second.lastActivity = std::chrono::steady_clock::now();
        // ✅ _tokens 업데이트 불필요!
    }
}
```

### 평가
**최적 선택** - 안전성, 일관성, 유지보수성 모두 우수

---

## 메모리 비교 (10,000 세션 기준)

| 옵션 | _tokens 크기 | 총 메모리 | 비고 |
|------|-------------|----------|------|
| Option 1 (포인터) | 80 KB | 최소 | ❌ 사용 불가 |
| Option 2 (값 복사) | 240 KB | +160 KB | ⚠️ 동기화 필요 |
| Option 3 (키 저장) | 280 KB | +200 KB | ✅ 권장 |

**결론**: 200KB 차이는 현대 서버에서 무시 가능 (L3 캐시보다 작음)

---

## 최종 권장사항

### 🏆 Option 3 (endpoint 키 저장) 선택 이유:

1. **안전성**: Dangling pointer 없음
2. **정확성**: 데이터 일관성 자동 보장 (Single Source of Truth)
3. **단순성**: 동기화 로직 불필요
4. **성능**: 2단계 조회 오버헤드 무시 가능 (나노초 수준)
5. **유지보수**: 버그 발생 가능성 최소화

### Option 2를 선택하려면:
다음 모든 메서드에서 _tokens 동기화 필요:
- `UpdateActivity()` ← 가장 빈번히 호출됨!
- `Register()`
- `RegisterWithToken()`
- `Remove()`

**동기화 누락 시**: 타임아웃 오판, 세션 조회 실패 등 미묘한 버그 발생

---

## 구현 변경 사항

### 현재 (Option 2)를 Option 3으로 변경:

```cpp
// UDPEndpointRegistry.h
- std::unordered_map<uint128_t, SessionInfo, System::uint128_hash> _tokens;
+ std::unordered_map<uint128_t, boost::asio::ip::udp::endpoint, System::uint128_hash> _tokens;

// UDPEndpointRegistry.cpp - RegisterWithToken
- _tokens[udpToken] = _sessions[endpoint];
+ _tokens[udpToken] = endpoint;

// UDPEndpointRegistry.cpp - GetEndpointByToken
ISession *UDPEndpointRegistry::GetEndpointByToken(uint128_t token)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto tokenIt = _tokens.find(token);
    if (tokenIt != _tokens.end())
    {
        auto sessionIt = _sessions.find(tokenIt->second);
        if (sessionIt != _sessions.end())
        {
            return sessionIt->second.session;
        }
    }
    return nullptr;
}
```

---

## 결론

**Option 3 (endpoint 키 저장)이 최선의 선택**

- 안전성과 정확성이 최우선
- 성능 차이는 측정 불가능한 수준
- 코드 복잡도는 약간 증가하지만 버그 위험은 대폭 감소
- "Premature optimization is the root of all evil" - 정확성 먼저, 최적화는 나중에
