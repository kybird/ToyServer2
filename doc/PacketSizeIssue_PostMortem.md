# 패킷 사이즈 할당 문제 포스트모텀 (Post-Mortem)

## 1. 문제 설명 (Problem Description)
VampireSurvivor 클라이언트가 `S_LOGIN` 패킷 수신 직후 연결이 멈추는 현상이 발생했습니다. 연결 자체는 성공했으나, 이후 `S_ENTER_LOBBY` 등의 후속 패킷을 수신하지 못하거나 처리하지 못하여 타임아웃 또는 무한 대기 상태에 빠졌습니다.

## 2. 원인 분석 (Root Cause Analysis)
문제의 원인은 서버 측에서 **네트워크 패킷 메모리 할당 시 크기 계산 오류**였습니다.

서버가 `MessagePool`에서 패킷 메모리를 할당할 때, 헤더 크기를 누락하고 바디(Body) 크기만큼만 할당하는 실수가 코드 전반에 존재했습니다.

**오류 코드 패턴:**
```cpp
// 기존 오류 코드
size_t bodySize = msg.ByteSizeLong();
// 치명적 오류: 헤더 크기를 더하지 않고 바디 크기만 할당함!
auto packet = System::MessagePool::AllocatePacket((uint16_t)bodySize); 

PacketHeader *header = (PacketHeader *)packet->Payload();
// 할당된 크기보다 더 큰 영역에 쓰거나(Buffer Overflow), 
// 헤더에 기록된 size보다 실제 메모리가 작아 클라이언트가 데이터를 덜 읽는 문제 발생.
header->size = (uint16_t)(sizeof(PacketHeader) + bodySize); 
```

`PacketHeader`에는 패킷 전체 크기(헤더+바디)가 기록되지만, 실제 전송되는 데이터는 바디 크기만 할당된 불완전한 덩어리였습니다.
*   **결과**: 네트워크 레이어는 할당된 크기만큼만 전송하거나, 잘못된 메모리 영역을 참조하게 됩니다. 클라이언트는 헤더에 명시된 `size`만큼 데이터를 기다리지만, 서버는 그보다 적은 데이터를 보내므로 영원히 나머지 바이트를 기다리게(Hang) 됩니다.

## 3. 임시 해결 및 리팩토링 (Immediate Fix)
이 문제를 해결하고 재발을 방지하기 위해 수동 계산 로직을 제거하고, 헬퍼 클래스를 도입했습니다.

### 3.1. 패킷 생성 표준화 (`PacketHelper`)
`src/Examples/VampireSurvivor/Server/Core/PacketHelper.h`를 생성하여 할당 로직을 캡슐화했습니다.

```cpp
class PacketHelper
{
public:
    template <typename ProtoMsg>
    static System::PacketMessage* MakePacket(uint16_t id, const ProtoMsg& msg)
    {
        size_t bodySize = msg.ByteSizeLong();
        
        // 수정됨: 항상 sizeof(PacketHeader)를 포함하여 할당
        auto packet = System::MessagePool::AllocatePacket((uint16_t)(sizeof(PacketHeader) + bodySize));
        
        if (packet)
        {
            PacketHeader* header = (PacketHeader*)packet->Payload();
            header->size = (uint16_t)(sizeof(PacketHeader) + bodySize);
            header->id = id;
            msg.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)bodySize);
        }
        return packet;
    }
};
```

### 3.2. 코드 수정
다음 파일들에서 수동 할당(`AllocatePacket`) 호출을 모두 `PacketHelper::MakePacket`으로 교체했습니다.
*   `GamePacketHandler.cpp` (로비, 룸, 채팅 처리)
*   `LoginController.cpp` (로그인 처리)
*   `WaveManager.cpp` (몬스터 스폰)
*   `Room.cpp` (브로드캐스트)

---

## 4. 제안: 프레임워크 레벨의 안전장치 (Proposed Framework Solution)
(이 제안은 5번 항목에서 실제로 구현되었습니다.)

## 5. 최종 구현: CRTP 패킷 시스템 (Final Implementation)
단순 헬퍼 함수를 넘어, 성능과 타입 안전성을 동시에 확보하기 위해 **CRTP (Curiously Recurring Template Pattern)** 기반의 패킷 시스템을 프레임워크 레벨(`System`)에 도입했습니다.

### 5.1. 구조
*   **PacketBase (CRTP)**: `System/Packet/PacketBase.h`에 구현. 컴파일 타임에 헤더 크기를 검증하고, `ToPacketMessage()`를 통해 안전하게 `PacketMessage*`로 변환합니다.
*   **PacketPool**: `System/Packet/PacketPool.h`에 구현. 패킷 래퍼 객체 자체를 재사용하여 오버헤드를 최소화합니다.
*   **GamePackets**: `GamePackets.h`에서 각 프로토콜 메시지(Protobuf)에 대응하는 패킷 클래스(`S_LoginPacket` 등)를 정의하여 사용합니다.

### 5.2. 사용 예시
```cpp
// 변경 전 (위험)
auto pkt = MessagePool::Allocate(msg.ByteSizeLong()); // 헤더 누락 위험!

// 변경 후 (안전하고 직관적)
S_LoginPacket packet(res);
session->Send(packet.ToPacketMessage());
```
이제 개발자는 단순히 패킷 클래스를 인스턴스화하기만 하면 되며, 크기 계산은 내부적으로 자동 처리됩니다.

## 6. 개발 중 발생한 문제 및 해결 (Issues & Resolutions)

### 6.1. 불완전한 타입(Incomplete Type) 오류
**문제**: `PacketHeader` 구조체 내부에서 `static constexpr size_t SIZE = sizeof(PacketHeader);`를 선언하자 컴파일 에러("invalid application of 'sizeof' to an incomplete type")가 발생했습니다.
**해결**: `SIZE` 상수의 **선언**은 구조체 내부에 두고, **정의**(`inline constexpr`)를 구조체 정의가 끝난 직후로 분리하여 해결했습니다.

### 6.2. 헤더 파일 참조 모호성 (Ambuous Include)
**문제**: `Protocol.h`가 `src/Share`와 `src/Examples/.../Server/Core` 두 곳에 존재하여, `GamePackets.h`가 의도치 않게 `Share/Protocol.h`를 참조했습니다. 이로 인해 `PacketHeader` 정의가 누락되어 컴파일 에러가 발생했습니다.
**해결**: `#include "Protocol.h"`를 `#include "Server/Core/Protocol.h"`로 변경하여 명시적으로 경로를 지정했습니다.

### 6.3. 템플릿 인자 이름 충돌 (Typo/Naming Clash)
**문제**: `S_MoveObjectBatchPacket` 템플릿 정의 시, `Protocol::S_MoveObjectBatch` (클래스) 대신 `Protocol::S_MOVE_OBJECT_BATCH` (Enum 상수)를 사용하여 타입 에러가 발생했습니다.
**해결**: Protobuf가 생성한 정확한 클래스 명(`S_MoveObjectBatch`)을 확인하여 수정했습니다.

### 6.4. Include Path 문제
**문제**: 클라이언트 프로젝트에서 `Server/Core/Protocol.h`를 찾지 못하는 문제가 있었습니다.
**해결**: CMake 설정 상 편의를 위해 `GamePackets.h` 내부의 include 경로를 조정하여, 클라이언트와 서버 양쪽에서 모두 컴파일 가능한 경로(`Server/Core/Protocol.h`)를 확정했습니다.
