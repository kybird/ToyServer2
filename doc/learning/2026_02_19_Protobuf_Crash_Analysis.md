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

## 💡 교훈 (Lessons Learned)
1.  **타입의 한계를 믿지 마라**: `uint16_t`를 쓰는 곳엔 반드시 `UINT16_MAX` 체크가 있어야 한다. `assert`도 믿지 말고 `static_assert`나 런타임 체크(`if`)를 적극 활용하자.
2.  **대규모 데이터는 무조건 쪼개라(Chunking)**: MMO에서 "모든 유저 정보", "모든 몬스터 정보"를 한 패킷에 담는 건 시한폭탄이다. 언제 터질지 모른다.
3.  **Fail Fast**: 조용히 잘려서 이상한 동작을 하는 것보다, 차라리 터져서 로그를 남기는 게 백배 낫다. (이번에도 터지지 않았다면 디버깅에 며칠이 걸렸을 것이다.)

## 🔗 관련 파일
- `doc/investigation/Crash_Root_Cause.md` (초기 분석 문서)
- `src/System/Packet/tests/CrashReproductionTests.cpp` (재현 및 검증 테스트 코드)
