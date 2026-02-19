# Protobuf 직렬화 크래시 사건 (The Great S_MoveObjectBatch Crash)

## 📅 발생 일시
- **2026-02-18** ~ **2026-02-19**

## 🎯 문제 (The Problem)
- **증상**: 게임 시작 후 약 10분이 지나 몬스터와 투사체(Projectile)가 수천 개에 도달했을 때, 서버가 `Segmentation Fault` 또는 `Heap Corruption`으로 갑자기 사망함.
- **원인 추적의 어려움**:
    - `Release` 빌드에서만 불규칙하게 발생.
    - 초기에는 `NaN`이나 `Infinity` 같은 부동소수점 오류, 혹은 멀티스레드 레이스 컨디션(Race Condition)을 의심했으나 모두 기각됨.

## 🕵️‍♂️ 원인 분석 (Root Cause Analysis)

### 1. **Integer Truncation (숫자 잘림 현상)**
- `PacketHeader::size`는 `uint16_t` (최대 **65,535**) 타입.
- 하지만 `PacketBase.h`의 `MaxPacketSize`는 **1MB (1,048,576)** 로 설정되어 있었음.
- **상황**:
    - 몬스터 2,500마리 위치 정보 패킷 크기 = **약 87,000 바이트**.
    - `CalculateSafeSize()` 함수가 이를 `uint16_t`로 반환하면서 `87,000 (0x153D8)` -> `21,464 (0x53D8)`로 **앞부분이 잘려나감**.
    - **결과**: `assert`는 1MB보다 작으니까 통과해버림.

### 2. **Heap Buffer Overflow (메모리 파괴)**
- `MessagePool`은 잘려나간 크기(**21KB**)만큼만 메모리를 할당함.
- 그러나 `SerializeToArray()`는 원본 크기(**87KB**)를 전부 다 씀.
- **결과**: 할당된 메모리를 넘어 **뒤쪽 66KB의 힙 메모리를 덮어써버림 (Heap Corruption)**. 이로 인해 서버가 즉시 죽거나, 엉뚱한 시점에 크래시 발생.

## 💊 해결 (The Fix)

### 1. **Safety Enforce (안전장치 강화)**
- **파일**: `src/System/Packet/PacketBase.h`
- **조치**: `MaxPacketSize`를 **65,535 (UINT16_MAX)** 로 강제 하향 조정.
- **효과**: 이제 65,535바이트를 넘는 패킷 생성 시, `assert` 실패 또는 `std::abort()`를 호출하여 **문제를 즉시 알림 (Fail Fast)**.

### 2. **Packet Chunking (패킷 분할)**
- **파일**: `src/Examples/VampireSurvivor/Server/Game/Room_Update.cpp`
- **조치**: `S_MoveObjectBatch` 패킷을 보낼 때, 한 번에 보내지 않고 **300개씩 끊어서(Chunking)** 여러 번 보냄.
- **효과**: 몬스터가 10만 마리가 되어도 패킷 하나는 절대 65KB를 넘지 않음. 네트워크 대역폭 관리에도 유리함.

---

## �️ [Part 2] 후속 사건: The `safeSize` Regression (2026-02-19)

### 1. 증상
- 위 1차 수정 직후, 클라이언트에서 **`Failed to parse C_LOGIN`** 에러 발생.
- 로그인 성공 후 **`S_RoomList` 패킷이 오지 않거나**, 엉뚱한 값으로 파싱됨.

### 2. 원인: 과잉 방어 (`safeSize`)
- **버그 코드**:
    ```cpp
    // Room_Update.cpp
    uint16_t safeSize = size + (size / 10) + 16;  // 여유분 10% + 16바이트 추가
    auto *msg = System::MessagePool::AllocatePacket(safeSize); 
    pkt.SerializeTo(msg->Payload());
    // msg->length를 size로 보정하지 않음!
    ```
- **문제점**:
    - `AllocatePacket(safeSize)`는 **`msg->length`를 `safeSize`(예: 32바이트)로 설정**함.
    - 실제 패킷(`size`)은 **15바이트**.
    - `GatewaySession::Flush`는 `msg->length`(32바이트)만큼 TCP로 전송.
    - **결과**: `[Header(4) + Body(11) + 쓰레기(17)]`가 전송됨.
    - 클라이언트는 앞의 15바이트만 읽고, **나머지 17바이트 쓰레기를 다음 패킷 헤더로 오인**하여 파싱 대실패.

### 3. 해결 및 교훈
- **해결**:
    - `safeSize` 로직 완전 제거 (테스트 결과 `ByteSizeLong()`이 정확함이 입증됨).
    - `AllocatePacket(pkt.GetTotalSize())`로 **할당 크기 = 전송 크기**를 일치시킴.
- **교훈**:
    - **Memory Allocation $\neq$ Network Transmission**: "넉넉하게 할당해도 된다"는 메모리 관점이고, 네트워크 전송 시엔 "정확하게 그만큼만" 보내야 한다.
    - **Verify Assumptions**: "Protobuf 계산이 틀릴 수도 있다"는 막연한 불안감(`safeSize`)이 오히려 버그를 만들었다. 테스트(`.CrashReproductionTests`)로 검증하고 코드를 짰어야 했다.

---

## 🔗 관련 파일
- `doc/investigation/Crash_Root_Cause.md` (초기 분석 문서)
- `src/System/Packet/tests/CrashReproductionTests.cpp` (재현 및 검증 테스트 코드)
