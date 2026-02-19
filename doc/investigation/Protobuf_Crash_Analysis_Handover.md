# Protobuf 직렬화 크래시 사건 수사 인계서 (Investigation Handover)

**일자**: 2026-02-18
**작성자**: Antigravity Assistant
**대상 이슈**: `S_MoveObjectBatch` 패킷 직렬화 (`SerializeToArray`) 중 크래시 발생.

---

## 🛑 현재 상태 (Status)
**[Safety Mechanism Disabled]**
`src/System/Packet/PacketBase.h`의 **안전 마진(10% 추가 할당) 코드가 비활성화(#ifdef로 주석 처리)**되어 있습니다.
- 현재 시스템은 `ByteSizeLong()`이 반환한 **정확한 크기**만큼만 버퍼를 할당하여 직렬화를 시도합니다.
- 만약 `ByteSizeLong()` 계산이 조금이라도 틀리면 즉시 **Heap Buffer Overflow**가 발생할 수 있는 상태입니다.

---

## ✅ 무죄 확정 (Cleared Suspects)
다음 가설들은 `src/System/Packet/tests/CrashReproductionTests.cpp`를 통해 **거짓(False)**임이 증명되었습니다. **더 이상 이쪽을 의심하지 마십시오.**

1.  **NaN / Infinity 값 주입**:
    - `float` 필드(x, y, vx, vy)에 `NaN`, `Inf`를 넣어도 `SerializeToArray`는 정상 작동함. (크래시 원인 아님)
2.  **Varint 크기 계산 오류**:
    - `int32` 필드에 음수(-1)를 넣어 Varint 크기를 최대(10바이트)로 만들어도, `ByteSizeLong()`은 이를 정확히 계산함.
3.  **대량 객체 처리 (Buffer Overflow)**:
    - 1,000개의 객체를 담아도 `ByteSizeLong()`과 실제 직렬화 크기는 **1바이트의 오차도 없이 일치**함.

---

## 🕵️‍♂️ 유력 용의자 (New Hypotheses for Next Session)
단위 테스트(싱글 스레드, 격리된 환경)에서는 재현되지 않았으므로, **환경적 요인**이 원인일 가능성이 매우 높습니다. 다음 세션에서는 이들을 집중 수사해야 합니다.

### 1. 경쟁 상태 (Race Condition) - [최우선 의심]
- **시나리오**: `SerializeToArray`가 돌고 있는 도중에, 다른 스레드(Game Logic Thread 등)가 `moves` 리스트에 접근하여 `add_moves()`나 `Clear()`를 호출했을 가능성.
- **근거**: `ByteSizeLong()` 계산 시점과 `SerializeToArray()` 실행 시점 사이에 데이터가 변경되면 크기 불일치로 버퍼 오버플로우 발생.
- **검증 방법**: 멀티스레드 스트레스 테스트 작성. (한 스레드는 계속 직렬화, 다른 스레드는 `Reset` 후 다시 채우기 반복)

### 2. 메모리 오염 (Use-After-Free)
- **시나리오**: 패킷 객체(`S_MoveObjectBatch`)가 이미 `delete` 되었거나, `MessagePool`로 반환된 상태에서 `SerializeToArray`가 호출됨.
- **근거**: 죽은 객체의 가상 함수 테이블(vtable)이나 멤버 변수를 참조하면 엉뚱한 주소로 점프하거나 쓰레기 값을 읽어 크래시 발생.
- **검증 방법**: ASan (Address Sanitizer)을 켜고 빌드하여 실행. (Windows에서도 MSVC 2019+부터 지원)

### 3. 패킷 헤더 공간 관리 실수 (Header Mismanagement)
- **시나리오**: `PacketBase`가 버퍼 포인터를 넘겨줄 때, `Header Size`만큼 **건너뛰고(offset)** 넘겨줘야 하는데, 실수로 헤더 영역부터 덮어쓰게 하여 뒤쪽 메모리를 침범했을 가능성.
- **근거**: 현재 `CrashReproductionTests`는 `PacketBase`를 통하지 않고 직접 버퍼를 다루거나, MockHeader를 썼기 때문에 실제 `System` 헤더 구조와 다를 수 있음.

---

## 🛠️ 다음 행동 지침 (Action Plan)

1.  **멀티스레드 재현 테스트 작성**:
    - `CrashReproductionTests.cpp`에 `Test 4: Concurrent Access` 추가.
    - `std::thread` 2개를 사용하여 Race Condition 유발 시도.
    
2.  **ASan (Address Sanitizer) 활성화**:
    - `CMakeLists.txt`에 `/fsanitize=address` 옵션 추가 후 `UnitTests` 실행.
    - Use-After-Free 잡기.

3.  **실제 서버 부하 테스트 (Integration Test)**:
    - `UdpSpamClient`를 사용하여 `S_MoveObjectBatch`를 대량으로 주고받으며 크래시 유도.
    - 이때 `PacketBase.h`의 Fix는 **계속 꺼둔 상태**여야 함.

---

**결론**: 라이브러리는 죄가 없습니다. **범인은 우리 코드의 동시성 제어(Concurrency Control)나 메모리 생명주기 관리(Life-cycle Management)에 있습니다.**
