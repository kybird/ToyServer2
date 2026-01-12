# 2026-01-12 개발 로그: 틱 동기화 버그 해결 (GameTick = 0.0)

## 🔥 문제 발견: "완벽과는 거리가 먼" 동기화

### 증상
```
[DeadReckoning] ID=Projectile_1001 | ServerPos=(799.09,0.00) | RenderPos=(798.30,0.00) | Diff=0.79
[DeadReckoning] ID=Monster_1001 | ServerPos=(-11.31,-4.20) | RenderPos=(-12.51,-4.20) | Diff=1.20
```

- **Diff 값이 0.75~1.51로 일정하게 유지** - 시간이 지나도 수렴하지 않음
- 보간 지연(50ms)을 고려해도 과도한 차이
- 프로젝타일과 몬스터 모두 동일한 패턴

---

## 🔍 디버깅 과정

### 1단계: 보간 지연 의심 ❌
**가설**: Delay가 너무 크다 (100ms → 50ms 축소)
- `INTERPOLATION_DELAY`: 100ms → 50ms
- `MIN_DELAY`: 50ms → 33ms (1틱)
- **결과**: Diff가 1.20 → 0.78로 감소했지만 여전히 일정하게 유지

### 2단계: EstimateServerTickFloat() 버그 수정 ✅
**발견**: `uint`로 캐스팅 후 `float` 변환하여 소수점 손실
```csharp
// 수정 전
public float EstimateServerTickFloat() => (float)EstimateGameTick(); // uint → float

// 수정 후
public float EstimateServerTickFloat()
{
    if (!_gameInitialized || TickRate <= 0) return 0f;
    double dt = Time.realtimeSinceStartupAsDouble - _baseGameTime;
    if (dt < 0) dt = 0;
    return _baseGameTick + (float)(dt * TickRate) + _gameTickOffset; // 소수점 유지
}
```
**결과**: 보간이 부드러워졌지만 Diff는 여전히 유지

### 3단계: 디버그 로그 추가 - 결정적 단서 발견! 🎯
```csharp
[TickSync] Monster_1002 | GameTick=0.0 | RenderTick=-0.9 | LastSnapTick=11478 | Behind=11478.9
```

**충격적인 발견**: `GameTick = 0.0` ← TickManager가 초기화되지 않음!
- 서버 틱: 11478
- 클라이언트 추정 틱: **0.0**
- Behind: **11478틱** (약 383초 = 6분 23초!)

---

## 🐛 근본 원인 분석

### 버그 체인 (Bug Chain)

#### 1. WaveManager에서 server_tick 누락
```cpp
// WaveManager.cpp::SpawnMonster() - 수정 전
Protocol::S_SpawnObject msg;
// ⚠️ server_tick 설정 안 함!
auto *info = msg.add_objects();
info->set_object_id(monster->GetId());
// ...
BroadcastProto(room, PacketID::S_SPAWN_OBJECT, msg);
```

#### 2. PacketHandler의 조건부 앵커링
```csharp
// PacketHandler.cs::Handle_S_SpawnObject()
if (TickManager.Instance != null && res.ServerTick > 0)  // ← ServerTick이 0이면 통과 안 됨!
{
    TickManager.Instance.InitGameAnchor(res.ServerTick);
}
```

#### 3. 플로우 차트
```
게임 시작
    ↓
플레이어 입장 (솔로 플레이)
    ↓
기존 오브젝트 없음 → S_SpawnObject(기존 오브젝트) 안 보냄
    ↓
몬스터 스폰 (WaveManager)
    ↓
S_SpawnObject 전송 (server_tick = 0)  ← 버그!
    ↓
클라이언트: res.ServerTick == 0
    ↓
InitGameAnchor() 호출 안 됨
    ↓
GameTick = 0.0 고정
    ↓
동기화 완전 실패 💥
```

### 왜 이전에 발견 못 했나?
- **멀티플레이어 테스트**: 두 번째 플레이어는 첫 번째 플레이어(기존 오브젝트)를 받아서 정상 작동
  ```cpp
  // Room.cpp::OnPlayerReady() - 여기는 server_tick이 설정되어 있었음
  Protocol::S_SpawnObject existingObjects;
  existingObjects.set_server_tick(_serverTick); // ✓
  ```
- **솔로 플레이만 하면 100% 재현**

---

## ✅ 해결 방법

### WaveManager.cpp 수정
```cpp
// Broadcast Spawn
Protocol::S_SpawnObject msg;
msg.set_server_tick(room->GetServerTick()); // [중요] 틱 동기화를 위해 server_tick 설정
auto *info = msg.add_objects();
info->set_object_id(monster->GetId());
info->set_type(Protocol::ObjectType::MONSTER);
info->set_type_id(monster->GetMonsterTypeId());
info->set_x(x);
info->set_y(y);
info->set_hp(monster->GetHp());
info->set_max_hp(monster->GetMaxHp());

BroadcastProto(room, PacketID::S_SPAWN_OBJECT, msg);
```

### 검증 결과
```
[TickSync] Monster_1001 | GameTick=2583.8 | RenderTick=2583.0 | LastSnapTick=2582 | Behind=-1.0
```

| 지표 | 수정 전 | 수정 후 |
|------|---------|---------|
| **GameTick** | 0.0 ❌ | 2583.8 ✅ |
| **Behind** | ~11500틱 ❌ | -1.0틱 ✅ |
| **Diff** | 1.20 (일정) | 0.3~0.5 (정상) |

**Behind = -1.0**은 정상입니다:
- 클라이언트가 RTT/2 보정으로 서버보다 약 1틱 앞서서 추정
- 새 스냅샷 도착 전까지 extrapolation 사용
- 로컬 환경이므로 RTT ≈ 0, 오차 누적 최소화

---

## 🔧 추가 개선 사항

### 1. TickManager 리셋 메커니즘 추가
게임 재시작 시 `_gameInitialized`가 리셋되지 않는 문제 방지:

```csharp
// TickManager.cs
public void ResetGameAnchor()
{
    _gameInitialized = false;
    _gameTickOffset = 0;
    Debug.Log("[TickManager] Game Anchor Reset. Ready for new game.");
}

// GameSceneInitializer.cs
void Start()
{
    // 이전 게임 오브젝트 정리
    if (ObjectManager.Instance != null)
    {
        ObjectManager.Instance.Clear();
    }

    // [중요] 틱 앵커 리셋 - 새 게임에서 올바른 틱 동기화를 위해 필수
    if (TickManager.Instance != null)
    {
        TickManager.Instance.ResetGameAnchor();
    }
}
```

### 2. 디버그 로그 개선
```csharp
// DeadReckoning.cs - 틱 동기화 상태 진단
float tickBehind = lastSnap.time - renderTick;
if (tickBehind > 3f || tickBehind < -1f)
{
    Debug.LogWarning($"[TickSync] {gameObject.name} | GameTick={currentGameTick:F1} | RenderTick={renderTick:F1} | LastSnapTick={lastSnap.time:F0} | Behind={tickBehind:F1}");
}
```

---

## 📝 교훈

### 1. "모든 경로에서 server_tick을 설정하라"
- `S_SpawnObject`, `S_MoveObjectBatch` 등 **모든 게임 상태 패킷**에 `server_tick` 필수
- 한 곳이라도 누락되면 전체 동기화 실패

### 2. "디버그 로그는 진실을 말한다"
- `GameTick=0.0`이라는 명확한 증거가 있었다면 즉시 발견 가능
- **핵심 지표를 항상 로깅**하라

### 3. "솔로/멀티 시나리오 모두 테스트하라"
- 멀티플레이어에서만 작동하는 버그는 발견하기 어려움
- **Edge Case (첫 플레이어, 빈 방)** 반드시 검증

### 4. "시간이 지나도 수렴하지 않으면 구조적 문제"
- Diff가 일정하게 유지 = 보간 문제가 아닌 **데이터 문제**
- 근본 원인을 찾을 때까지 계속 파고들어야 함

---

## 🚀 향후 개선 사항

### 소프트 틱 보정 (Soft Tick Correction)
현재는 틱 앵커를 한 번만 설정하므로, 시간이 지나면서 미묘한 드리프트 발생 가능:
- `Behind = -1.0 → -1.1 → -1.2 ...` (시간 추정 오차 누적)

**해결책**: 주기적으로 `CheckAndCorrectGameAnchor()` 호출
```csharp
// PacketHandler.cs::Handle_S_MoveObjectBatch()에 추가 고려
if (TickManager.Instance != null)
{
    TickManager.Instance.CheckAndCorrectGameAnchor(res.ServerTick, NetworkManager.Instance.RTT);
}
```

**고려사항**:
- 매 프레임 호출 시 오버헤드 (~10μs, 무시 가능)
- 초당 1회로 제한 가능
- 몬스터/프로젝타일 수백~수천 개여도 패킷당 1회만 실행

---

*"버그는 항상 가장 기본적인 곳에 숨어있다. server_tick 하나가 모든 것을 망가뜨렸다."*

---

## 🚀 기술 블로그 포스팅 초안

# 멀티플레이어 게임의 시간 여행 버그: GameTick=0.0의 공포

## 💡 오늘의 도전: "왜 몬스터가 저 멀리 있어?"

뱀파이어 서바이버 스타일 게임을 개발하던 중, 이상한 현상을 마주했습니다. 몬스터와 프로젝타일이 **서버 위치보다 항상 1.2 유닛 뒤에** 렌더링되는 것이었죠. 보간 지연을 50ms로 줄여도, 네트워크 최적화를 해도 변하지 않는 일정한 차이. 무언가 근본적으로 잘못되었다는 직감이 들었습니다.

```
[DeadReckoning] ID=Monster_1001 | ServerPos=(-11.31,-4.20) | RenderPos=(-12.51,-4.20) | Diff=1.20
```

그리고 디버그 로그를 추가한 순간, 충격적인 진실을 마주했습니다:

```
[TickSync] Monster_1002 | GameTick=0.0 | RenderTick=-0.9 | LastSnapTick=11478 | Behind=11478.9
```

**클라이언트가 시간을 잃어버렸습니다.** 서버는 11,478틱(약 6분 후)에 있는데, 클라이언트는 여전히 0틱. 마치 시간이 멈춘 것처럼.

---

## 🛠 해결 과정: 단 한 줄의 누락이 만든 재앙

### 1단계: 잘못된 가설들과의 싸움

처음엔 보간 지연이 문제라고 생각했습니다. 하지만 `INTERPOLATION_DELAY`를 100ms에서 50ms로 줄여도 소용없었죠. 그다음 의심한 건 `EstimateServerTickFloat()` 함수였습니다.

**발견한 첫 번째 버그**: 소수점 손실
```csharp
// 수정 전: uint로 캐스팅 후 float 변환 - 소수점 손실!
public float EstimateServerTickFloat() => (float)EstimateGameTick();

// 수정 후: 소수점 정밀도 유지
public float EstimateServerTickFloat()
{
    if (!_gameInitialized || TickRate <= 0) return 0f;
    double dt = Time.realtimeSinceStartupAsDouble - _baseGameTime;
    if (dt < 0) dt = 0;
    return _baseGameTick + (float)(dt * TickRate) + _gameTickOffset;
}
```

보간이 부드러워졌지만, 근본 문제는 해결되지 않았습니다.

### 2단계: 디버깅의 힘 - "GameTick=0.0"의 발견

```csharp
// 핵심 지표 로깅 추가
float tickBehind = lastSnap.time - renderTick;
Debug.LogWarning($"[TickSync] {gameObject.name} | GameTick={currentGameTick:F1} | RenderTick={renderTick:F1} | LastSnapTick={lastSnap.time:F0} | Behind={tickBehind:F1}");
```

이 간단한 로그가 모든 것을 밝혀냈습니다. `GameTick=0.0` - 틱 매니저가 초기화되지 않았던 겁니다!

### 3단계: 범인을 찾아서 - 개발자의 잘못된 가정

**진짜 문제는 코드가 아니라 가정이었습니다.**

개발자(저)는 이렇게 생각했습니다:
```
"C_GameReady를 보내면, OnPlayerReady()에서 
기존 오브젝트를 먼저 보낼 거야. 
그 패킷에 server_tick이 있으니 초기화는 문제없겠지."
```

**실제로는**:
```
C_GameReady 전송
    ↓
OnPlayerReady() 호출
    ↓
기존 오브젝트 있나? → 없음 (솔로 플레이)
    ↓
StartGame() → WaveManager 시작
    ↓
WaveManager가 몬스터 스폰 ← 첫 S_SpawnObject!
    ↓
server_tick=0 인 패킷 도착
    ↓
InitGameAnchor 호출 안 됨 💥
```

**코드 증거**:
```cpp
// Room.cpp::OnPlayerReady() - 여기만 server_tick 설정
Protocol::S_SpawnObject existingObjects;
existingObjects.set_server_tick(_serverTick); // ✓ 설정됨

// WaveManager.cpp::SpawnMonster() - 여기는 설정 안 함!
Protocol::S_SpawnObject msg;
// ⚠️ server_tick 누락!
auto *info = msg.add_objects();
info->set_object_id(monster->GetId());
// ...
BroadcastProto(room, PacketID::S_SPAWN_OBJECT, msg);

// 수정 후 - 단 한 줄의 추가
Protocol::S_SpawnObject msg;
msg.set_server_tick(room->GetServerTick()); // 🎯 이게 전부였다!
```

**왜 이전엔 발견 못 했나요?**
- 멀티플레이어: 두 번째 플레이어는 첫 번째 플레이어를 "기존 오브젝트"로 받아서 정상 작동
- **솔로 플레이만 하면 100% 재현!**

**교훈**: "첫 패킷은 예측할 수 없다" - 모든 경로에 필수 데이터를 설정하라.

---

## 📝 배운 점 & 인사이트

### 1. **"모든 경로에 server_tick을"**
네트워크 게임에서 시간 동기화는 **모든 곳에서** 일관되게 처리되어야 합니다. 한 곳이라도 누락되면 전체 시스템이 무너집니다. `S_SpawnObject`, `S_MoveObjectBatch`, `S_DamageEffect` - **모든 게임 상태 패킷에 `server_tick`을 필수로 포함**하세요.

### 2. **"디버그 로그는 거짓말하지 않는다"**
"뭔가 이상하다"는 느낌이 들 때, **핵심 지표를 로깅**하세요. `GameTick=0.0`이라는 명확한 증거만 있었다면 몇 시간을 절약할 수 있었을 겁니다.

```csharp
// 항상 핵심 지표를 로깅하라
Debug.Log($"GameTick={currentTick}, ServerTick={lastSnap.tick}, Diff={diff}");
```

### 3. **"Edge Case를 놓치지 마라"**
솔로 플레이, 멀티 플레이, 재접속, 늦은 조인... **모든 시나리오를 테스트**하세요. 멀티에서만 동작하는 버그는 치명적입니다.

### 4. **"일정한 오차는 데이터 문제다"**
보간/외삽 문제는 시간에 따라 **변동**합니다. 하지만 Diff가 항상 1.2로 일정하다면? 그건 알고리즘이 아닌 **데이터가 잘못된 겁니다**. 근본 원인을 찾을 때까지 파고드세요.

---

## 🎓 기술적 교훈

**멀티플레이어 게임의 시간 동기화는 분산 시스템의 클럭 동기화 문제**입니다. 각 클라이언트는 독립적인 시계를 가지지만, 서버의 권한 시간(Authoritative Time)에 동기화되어야 합니다.

**Best Practices**:
1. **명시적 타임스탬프**: 모든 게임 상태 패킷에 `server_tick` 포함
2. **초기화 검증**: 틱 앵커가 올바르게 설정되었는지 반드시 확인
3. **드리프트 보정**: 주기적으로 서버 시간과 재동기화 (`CheckAndCorrectGameAnchor`)
4. **포괄적 테스트**: 솔로/멀티/재접속 모든 시나리오 검증

---

### 검증 결과

| 지표 | 수정 전 | 수정 후 |
|------|---------|---------|
| **GameTick** | 0.0 ❌ | 2583.8 ✅ |
| **Behind** | ~11500틱 (6분!) | -1.0틱 (정상) |
| **Diff** | 1.20 (일정) | 0.3~0.5 (자연스러운 변동) |

---

**결론**: 때로는 몇 시간의 디버깅이 **단 한 줄의 코드**로 끝납니다. 하지만 그 한 줄을 찾기 위해선 시스템에 대한 깊은 이해와 체계적인 접근이 필요합니다. 

*"버그는 항상 가장 기본적인 곳에 숨어있다. server_tick 하나가 모든 것을 망가뜨렸다."*
