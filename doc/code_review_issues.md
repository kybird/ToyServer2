# 코드 리뷰 이슈 보고서

**날짜**: 2024
**대상**: ToyServer2 System 프레임워크
**검토 범위**: src/System 전체

---

## 🔴 Critical 심각도 (즉시 수정 필요)

### 1. UDPEndpointRegistry - Dangling Pointer 위험
**파일**: `src/System/Network/UDPEndpointRegistry.cpp:88-89`

**문제**:
```cpp
_tokens[session->GetUdpToken()] = &it->first;  // ❌ unordered_map 요소의 포인터 저장
```

`unordered_map`이 rehash될 때 내부 요소들이 재배치되면서 저장된 포인터가 무효화됨.

**영향**: 런타임 크래시, 메모리 손상

**해결 방법**:
```cpp
// _tokens 타입 변경: map<uint128_t, endpoint*> -> map<uint128_t, endpoint>
_tokens[session->GetUdpToken()] = it->first;  // 값 복사

// GetEndpointByToken에서 2단계 조회
auto tokenIt = _tokens.find(token);
if (tokenIt != _tokens.end()) {
    auto sessionIt = _sessions.find(tokenIt->second);
    // ...
}
```

---

### 2. SessionFactory - UDPSession 메모리 누수
**파일**: `src/System/Session/SessionFactory.cpp:113-123`

**문제**:
```cpp
void SessionFactory::Destroy(ISession *session)
{
    if (session->GetType() == SessionType::TCP) {
        // TCP 세션만 처리
        _tcpPool.push(static_cast<AsioSession *>(session));
    }
    // ❌ UDPSession은 처리 안 됨 -> 메모리 누수
}
```

`CreateUDPSession()`에서 `new UDPSession()`으로 할당했지만 해제 로직 없음.

**영향**: 메모리 누수, 장시간 실행 시 메모리 고갈

**해결 방법**:
```cpp
void SessionFactory::Destroy(ISession *session)
{
    if (session->GetType() == SessionType::TCP) {
        _tcpPool.push(static_cast<AsioSession *>(session));
    }
    else if (session->GetType() == SessionType::UDP) {
        // Option 1: 풀링
        // _udpPool.push(static_cast<UDPSession *>(session));
        
        // Option 2: 직접 해제 (풀 없을 경우)
        delete static_cast<UDPSession *>(session);
    }
}
```

---

## 🟠 High 심각도 (우선 수정 권장)

### 3. UDPSession - Raw Pointer Dangling 위험
**파일**: `src/System/Session/UDPSession.cpp:13-14`

**문제**:
```cpp
UDPNetworkImpl *network = nullptr;  // ❌ Raw pointer
```

`UDPNetworkImpl`이 먼저 소멸되면 `UDPSession`이 dangling pointer 접근.

**영향**: Use-after-free, 크래시

**해결 방법**:
```cpp
// Option 1: weak_ptr 사용
std::weak_ptr<UDPNetworkImpl> network;

// Option 2: 명시적 생명주기 관리
// UDPSession 소멸 시 network를 nullptr로 설정하는 observer 패턴
```

---

### 4. UDPSession - 이중 참조 카운트 감소
**파일**: `src/System/Session/UDPSession.cpp:135-137`

**문제**:
```cpp
msg->DecRef();           // ❌ 첫 번째 감소
if (msg->DecRef())       // ❌ 두 번째 감소 - 버그!
{
    if (msg->isPooled) {
    }
}
```

참조 카운트를 두 번 감소시켜 조기 해제 또는 이중 해제 발생.

**영향**: Use-after-free, 이중 해제, 크래시

**해결 방법**:
```cpp
// Option 1: 한 번만 호출
if (msg->DecRef()) {
    if (msg->isPooled) {
        MessagePool::Free(msg);
    }
}

// Option 2: 반환값 확인 후 처리
bool shouldDelete = msg->DecRef();
if (shouldDelete && msg->isPooled) {
    MessagePool::Free(msg);
}
```

---

### 5. UDPNetworkImpl - NAT Rebinding 시 레지스트리 미업데이트
**파일**: `src/System/Network/UDPNetworkImpl.cpp:158-163`

**문제**:
NAT rebinding 감지 시 새 endpoint를 레지스트리에 업데이트하지 않음.

**영향**: 세션 조회 실패, 패킷 라우팅 오류

**해결 방법**:
```cpp
if (sessionByToken && !sessionByEndpoint) {
    // NAT rebinding detected
    LOG_INFO("NAT rebinding detected for token {}", token);
    
    // ✅ 레지스트리 업데이트
    _registry->UpdateEndpoint(token, remoteEndpoint);
    
    sessionByToken->UpdateActivity();
    return sessionByToken;
}
```

---

## 🟡 Medium 심각도 (개선 권장)

### 6. SessionFactory - 프로덕션 코드에 std::cout 사용
**파일**: `src/System/Session/SessionFactory.cpp:33-48`

**문제**:
```cpp
std::cout << "[DEBUG] Pool size: " << _tcpPool.size() << std::endl;  // ❌
```

**해결 방법**:
```cpp
LOG_DEBUG("Pool size: {}", _tcpPool.size());  // ✅
```

---


### 8. UDPSession - 빈 if 블록
**파일**: `src/System/Session/UDPSession.cpp:138-141`

**문제**:
```cpp
if (msg->isPooled) {
    // ❌ 빈 블록 - 미완성 구현
}
```

**해결 방법**:
```cpp
if (msg->isPooled) {
    MessagePool::Free(msg);  // ✅ 풀 반환
}
```

---

## ✅ 해결됨 (Resolved)

### 7. UDPSession - 핫패스에서 동적 메모리 할당
**상태**: **수정 완료** (Flush 함수 리팩토링으로 해결)
- `MessagePool` 및 `AsyncSend`로 Zero-Copy 전송 구현 완료.
- 단, `SendReliable`의 1024바이트 초과 패킷 처리는 별도 최적화 필요 (TODO 등록됨).

---

## ✅ 문제 없음 (False Positive)

다음 항목들은 이전 단일 파일 검토에서 문제로 지적되었으나, 전체 맥락 분석 결과 **정상**으로 판명:

### DispatcherImpl.cpp
1. **Out-of-bounds 읽기** - ✅ `try_dequeue_bulk` 반환값으로 정확히 범위 제한
2. **reinterpret_cast 정렬** - ✅ MessagePool의 고정 크기 할당으로 정렬 보장
3. **Wait 예외 안전성** - ✅ condition_variable 예외는 극히 드물며, 발생 시 종료가 적절
4. **메시지 루프 예외 안전성** - ✅ 핸들러 예외는 치명적 버그이므로 fail-fast 의도
5. **Post null 검증** - ✅ 내부 API로 assert로 충분 (권장: `assert(message)` 추가)
6. **Moved-from 객체 사용** - ✅ 람다 캡처 초기화 정상 패턴

---

## 우선순위 수정 순서

1. **즉시**: Critical #1, #2 (메모리 안전성)
2. **이번 주**: High #3, #4, #5 (런타임 안정성)
3. **다음 스프린트**: Medium #6, #7, #8 (코드 품질)

---

## 추가 권장 사항

### 코딩 컨벤션 준수 체크리스트
- [x] 인터페이스/구현 분리 (I 접두어)
- [x] 의존성 주입 패턴
- [x] RAII 및 소유권 관리
- [x] 핫패스 가이드라인 (UDPSession::Flush 위반)
- [x] Manager 네이밍 지양

### 테스트 추가 필요 (todo.md 로 이관됨)
- [ ] UDP 세션 생명주기 테스트 (`todo.md` 참조)
- [ ] NAT rebinding 시나리오 테스트 (`todo.md` 참조)
- [ ] 메모리 누수 검증 (Valgrind/AddressSanitizer -> CrtDbgFlag) (`todo.md` 참조)
