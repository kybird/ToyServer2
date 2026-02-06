# SessionFactory 설계 문제 분석

## 발견된 문제들

### 1. Factory가 Destroy를 담당 (안티패턴)
```cpp
// 현재 구조
SessionFactory::Create()   // Factory가 생성
SessionFactory::Destroy()  // Factory가 소멸도 담당 ❌

// 문제점
- Factory는 "생성"만 책임져야 함 (Single Responsibility)
- Destroy 로직이 Factory에 있으면 타입별 if 지옥
- 새 세션 타입 추가 시 Factory 수정 필요 (Open-Closed 위반)
```

### 2. UDP 세션만 풀링 안 함 (불일치)
```cpp
// TCP 세션
GatewaySession *sess = pool.Acquire();  // ✅ 풀링
pool.Release(sess);                     // ✅ 풀링

// UDP 세션
UDPSession *sess = new UDPSession();    // ❌ new
delete sess;                            // ❌ delete

// 문제점
- 일관성 없음
- UDP 세션 생성/소멸 비용 높음
- 메모리 단편화
```

### 3. dynamic_cast 사용 (성능 문제)
```cpp
if (auto *udpSession = dynamic_cast<UDPSession *>(session))
{
    delete udpSession;
}

// 문제점
- RTTI 오버헤드 (가상 함수 테이블 조회)
- 타입 추가 시마다 if 추가
- 컴파일 타임에 잡을 수 없는 버그
```

---

## 더 나은 설계 패턴

### Option 1: Virtual Destructor + Self-Destruction (권장)

#### 개념
```cpp
// 각 세션이 자신의 소멸 방법을 알고 있음
class ISession {
    virtual void Destroy() = 0;  // 자기 자신을 소멸
};
```

#### 구현
```cpp
// ISession.h
class ISession {
public:
    virtual ~ISession() = default;
    virtual void Destroy() = 0;  // 자기 소멸 책임
};

// GatewaySession.h
class GatewaySession : public Session {
public:
    void Destroy() override {
        OnRecycle();  // 정리
        GetSessionPool<GatewaySession>().Release(this);
    }
};

// BackendSession.h
class BackendSession : public Session {
public:
    void Destroy() override {
        OnRecycle();
        GetSessionPool<BackendSession>().Release(this);
    }
};

// UDPSession.h
class UDPSession : public Session {
public:
    void Destroy() override {
        OnRecycle();
        GetSessionPool<UDPSession>().Release(this);  // 풀링 추가!
    }
};

// 사용
void DispatcherImpl::ProcessPendingDestroys() {
    for (auto *session : _pendingDestroy) {
        if (session->CanDestroy()) {
            session->Destroy();  // ✅ 각자 알아서 소멸
        }
    }
}
```

#### 장점
- ✅ Factory에서 if 제거
- ✅ 새 세션 타입 추가 시 Factory 수정 불필요
- ✅ 각 세션이 자신의 생명주기 관리
- ✅ 컴파일 타임 안전성

---

### Option 2: Type Erasure + Deleter

#### 개념
```cpp
// std::shared_ptr의 custom deleter 패턴
std::shared_ptr<ISession> session(
    new GatewaySession(),
    [](ISession *s) { /* custom delete */ }
);
```

#### 구현
```cpp
// SessionFactory.h
class SessionFactory {
    using SessionDeleter = std::function<void(ISession*)>;
    
    static std::pair<ISession*, SessionDeleter> CreateSession(...);
};

// SessionFactory.cpp
auto SessionFactory::CreateSession(...) 
    -> std::pair<ISession*, SessionDeleter>
{
    if (_serverRole == ServerRole::Gateway) {
        auto *sess = pool.Acquire();
        auto deleter = [](ISession *s) {
            GetSessionPool<GatewaySession>().Release(
                static_cast<GatewaySession*>(s)
            );
        };
        return {sess, deleter};
    }
    // ...
}

// 사용
auto [session, deleter] = SessionFactory::CreateSession(...);
// ...
deleter(session);  // 생성 시 정해진 방법으로 소멸
```

#### 장점
- ✅ Factory가 생성과 소멸 방법을 함께 제공
- ✅ Type-safe
- ⚠️ 단점: Deleter 저장 필요

---

### Option 3: Pooling 통일 (가장 단순)

#### 개념
```cpp
// 모든 세션을 풀링으로 통일
template<typename T>
SessionPool<T>& GetSessionPool();
```

#### 구현
```cpp
// SessionFactory.cpp
ISession *SessionFactory::CreateUDPSession(...) {
    auto& pool = GetSessionPool<UDPSession>();
    UDPSession *sess = pool.Acquire();  // ✅ 풀링
    sess->Reset(...);
    return sess;
}

void SessionFactory::Destroy(ISession *session) {
    if (!session) return;
    
    if (auto *udp = dynamic_cast<UDPSession*>(session)) {
        GetSessionPool<UDPSession>().Release(udp);  // ✅ 풀링
        return;
    }
    
    if (_serverRole == ServerRole::Gateway) {
        GetSessionPool<GatewaySession>().Release(...);
    } else {
        GetSessionPool<BackendSession>().Release(...);
    }
}
```

#### 장점
- ✅ 일관성 (모든 세션 풀링)
- ✅ 성능 향상 (new/delete 제거)
- ⚠️ 여전히 dynamic_cast 필요

---

## 권장 해결 방안

### Phase 1: 즉시 수정 (메모리 누수 해결)
```cpp
// 현재 코드에 UDP 풀링만 추가
ISession *SessionFactory::CreateUDPSession(...) {
    auto& pool = GetSessionPool<UDPSession>();
    UDPSession *sess = pool.Acquire();
    sess->Reset(...);
    return sess;
}

void SessionFactory::Destroy(ISession *session) {
    if (!session) return;
    
    if (auto *udp = dynamic_cast<UDPSession*>(session)) {
        GetSessionPool<UDPSession>().Release(udp);
        return;
    }
    
    // 기존 TCP 로직...
}
```

### Phase 2: 리팩토링 (설계 개선)
```cpp
// ISession에 Destroy() 추가
class ISession {
    virtual void Destroy() = 0;
};

// 각 세션 구현
class GatewaySession : public Session {
    void Destroy() override {
        GetSessionPool<GatewaySession>().Release(this);
    }
};

// Factory에서 Destroy 제거
// SessionFactory::Destroy() 삭제!

// 사용처 변경
// Before
SessionFactory::Destroy(session);

// After
session->Destroy();
```

---

## 다른 프레임워크 사례

### Boost.Asio
```cpp
// 각 객체가 자신의 생명주기 관리
class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
    void start() { /* ... */ }
    void stop() { /* 자기 자신 정리 */ }
};
```

### Unreal Engine
```cpp
// UObject가 자신의 소멸 관리
class UObject {
    virtual void BeginDestroy();  // 자기 소멸 로직
};
```

### Unity (C#)
```cpp
// MonoBehaviour가 자신의 생명주기 관리
class MyComponent : MonoBehaviour {
    void OnDestroy() { /* 자기 정리 */ }
}
```

---

## 최종 권장사항

### 즉시 (메모리 누수 수정)
1. ✅ UDP 세션 풀링 추가
2. ✅ Destroy()에서 UDP 처리

### 다음 리팩토링 시
1. 🔄 ISession::Destroy() 가상 함수 추가
2. 🔄 SessionFactory::Destroy() 제거
3. 🔄 각 세션이 자기 소멸 책임

### 설계 원칙
- **Single Responsibility**: Factory는 생성만
- **Open-Closed**: 새 타입 추가 시 기존 코드 수정 불필요
- **Polymorphism**: 가상 함수로 타입별 동작 분리

---

## 코드 비교

### Before (현재)
```cpp
// Factory가 모든 타입 알아야 함
void SessionFactory::Destroy(ISession *session) {
    if (dynamic_cast<UDPSession*>(session)) { /* ... */ }
    else if (_serverRole == Gateway) { /* ... */ }
    else { /* ... */ }
}
```

### After (권장)
```cpp
// Factory는 생성만
ISession *SessionFactory::Create(...) { /* ... */ }

// 각 세션이 자기 소멸
class GatewaySession {
    void Destroy() override { pool.Release(this); }
};

// 사용
session->Destroy();  // ✅ 간단!
```

---

## 결론

**Q: 이게 좋은 패턴이야?**

**A: 아니요. 여러 문제가 있습니다:**

1. ❌ Factory가 Destroy 담당 (책임 과다)
2. ❌ UDP만 풀링 안 함 (일관성 없음)
3. ❌ dynamic_cast 사용 (성능 + 확장성)
4. ❌ 타입 추가 시 Factory 수정 필요

**권장 해결책:**
- 즉시: UDP 풀링 추가
- 장기: ISession::Destroy() 가상 함수로 리팩토링
