# 유니티 클라이언트 구현 가이드 (Unity Client Implementation Guide)

이 문서는 서버와 공유된 `game.proto`를 기반으로 유니티 클라이언트를 독립적으로 개발하기 위한 아키텍처 및 연동 규격을 정의합니다.

## 1. 개요
서버는 권위적(Authoritative)으로 동작하며, 클라이언트는 서버에서 전달받은 상태(State)를 시각화하고 유저의 입력을 패킷으로 전달하는 역할을 수행합니다.

## 2. 클라이언트 핵심 구조

### NetworkManager (TCP/Protobuf)
서버와의 연결을 유지하고 바이너리 패킷을 직렬화/역직렬화합니다.
- **Packet Structure**: `[2 byte Size][2 byte PacketID][Protobuf Body]`
- **PacketSession**: 수신된 바이트 스트림을 헤더 기반으로 잘라내고, Protobuf 객체로 변환하여 로직단에 전달합니다.
- **MainThreadDispatcher**: 네트워크 스레드에서 수신된 패킷을 유니티 메인 스레드에서 안전하게 처리할 수 있도록 큐잉합니다.

### ObjectManager
서버에 존재하는 객체(Player, Monster, Projectile)들을 로컬에서 관리합니다.
- **State Table**: `Dictionary<int32, GameObject>` (Key: ObjectId)
- **Spawn/Despawn**: `S_SPAWN_OBJECT`와 `S_DESPAWN_OBJECT` 패킷에 대응하여 프리팹을 생성하거나 제거합니다.

## 3. 주요 동기화 시나리오

| 구분 | 관련 패킷 | 클라이언트 처리 내용 |
| :--- | :--- | :--- |
| **로그인** | `C_LOGIN` -> `S_LOGIN` | 서버 접속 후 자신의 PlayerId를 할당받고 월드 진입 준비. |
| **객체 생성** | `S_SPAWN_OBJECT` | 주변에 나타난 객체의 타입(Monster, Projectile 등)을 확인하여 프리팹 생성. |
| **객체 이동** | `S_MOVE_OBJECT_BATCH` | 서버에서 정의한 위치와 속도값으로 오브젝트 이동. (부드러운 연출을 위해 `Lerp` 권장) |
| **전투 발생** | `S_DAMAGE_EFFECT` | 적중된 몬스터 주변에 데미지 수치 팝업 및 피격 이펙트 재생. |
| **객체 소멸** | `S_DESPAWN_OBJECT` | 사망한 몬스터나 사라진 투사체를 화면에서 제거. |

## 4. 연동 시 주의사항

1. **좌표계**: 서버는 2D 평면 좌표(X, Y)를 사용합니다. 유니티의 (X, Y) 또는 (X, Z) 평면에 맞게 매핑이 필요합니다.
2. **패킷 ID**: `Protocol.cs`에 정의된 Enum 값과 클라이언트의 핸들러가 일치해야 합니다.
3. **리소스 매핑**: 서버의 `monster_type_id`와 클라이언트 프리팹 간의 매칭 데이터가 필요합니다.
