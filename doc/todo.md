# Project TODO List

## 🚀 Tasks In-Progress & Remaining

### Critical Optimization Tasks (Next Sprint)
- [ ] **[CRITICAL] Multi-Level MessagePool 확장**
    - 현재 `MessagePool`은 4KB 초과 시 `System Malloc`으로 우회하여 전역 힙 락 경합 유발.
    - 9KB ~ 2MB 대형 패킷 처리를 위해 **계층형 풀(Small/Medium/Large)** 도입 필요.
    - `SimpleMessage` 할당 전략을 개선하여 대용량 트래픽(3000 CCU) 대응.
- [ ] **[CRITICAL] Lock-Free Entity Pooling (SimplePool 교체)**
    - 현재 `ProjectileFactory`, `MonsterFactory`가 느린 `SimplePool` (`std::mutex` 기반) 사용 중.
    - 투사체/몬스터 대량 생성 시 심각한 락 경합 예상.
    - `LockFreeObjectPool` (`concurrentqueue` 기반) 도입 및 `Factory` 포팅 필요.
- [ ] **[HIGH] GatewaySession 버퍼 최적화 (Zero-fill 제거)**
    - 수신 버퍼 `resize()` 시 발생하는 불필요한 0 초기화 제거 (`reserve` + `push_back` 활용).
    - 초당 수십 GB의 메모리 쓰기 부하 제거.

---







---

## 🔧 Technical Debt (from Weapon Level-Up Mechanics Work)

### Critical
- [ ] **[CRITICAL] Protobuf 직렬화 크래시 (S_MoveObjectBatch) - 2024-01-XX**
    - **파일**: `src/System/Packet/PacketBase.h:108`, `src/Examples/VampireSurvivor/Server/Game/Room_Update.cpp:342`
    - **증상**: `WriteLittleEndian32ToArray` → `WriteFloatToArray` → `ObjectPos::_InternalSerialize` 크래시
    - **원인**: GameObject의 float 필드(x, y, vx, vy)에 NaN/Inf 값 존재 또는 버퍼 오버플로우
    - **적용된 수정**:
        1. `SyncNetwork()`에서 `std::isfinite()` 검증 추가
        2. `MessagePool::AllocatePacket` 크기를 `ByteSizeLong() + 10% + 16` 여유분 확보
        3. `PacketBase.h::SerializeBodyTo` 안전성 강화
    - **후속 조치**: 크래시 재현 시 NaN/Inf 근본 원인 추적 또는 패킷 분할 전송(100개/패킷) 구현

- [ ] **[CRITICAL] CombatManager 레벨 인덱스 접근 버그 수정**
    - **파일**: `src/Examples/VampireSurvivor/Server/Game/CombatManager.cpp:161`
    - **문제**: `weapon.levels[playerWeaponLevel - 1]` 인덱스 접근 사용
    - **영향**: sparse level 배열 (예: [1, 8]만 있는 경우)에서 out-of-bounds 접근 → 크래시
    - **해결**: T3에서 `Player::RefreshInventoryEffects`처럼 level 값으로 검색하도록 변경 필요

### Warning
- [ ] **[MEDIUM] Projectile::_hitTargets 벡터 무한 증가 방지**
    - **파일**: `src/Examples/VampireSurvivor/Server/Entity/Projectile.h`
    - **문제**: 무한 관통 투사체(`pierce = -1`)가 `_hitTargets`에 계속 ID 추가
    - **영향**: 장시간 생존하는 Orbit 투사체에서 메모리 무한 증가
    - **해결**: 최대 크기 제한 또는 순환 버퍼 사용 고려

- [ ] **[MEDIUM] EffectManager 몬스터 사망 시 effect 정리 확인**
    - **파일**: `src/Examples/VampireSurvivor/Server/Game/Effect/EffectManager.cpp`
    - **문제**: 몬스터 사망 시 해당 몬스터의 effect가 map에서 제거되는지 확인 필요
    - **영향**: 메모리 누수 가능성
    - **확인**: `Monster::OnDeath` → EffectManager 정리 로직 점검

### Info / Documentation
- [ ] **[LOW] Orbit speedMult 의미 문서화**
    - **파일**: `src/Examples/VampireSurvivor/Server/Game/DamageEmitter.cpp:317`
    - **내용**: Orbit의 `speedMult`는 각속도(rad/s), Linear의 `speedMult`는 선형 속도
    - **조치**: 무기 데이터 작성 가이드에 명시 필요

- [ ] **[LOW] Zone maxTargets 의미 명확화**
    - **파일**: `src/Examples/VampireSurvivor/Server/Game/DamageEmitter.cpp:357-358`
    - **내용**: Zone에서 `maxTargets`는 "타격할 몬스터 수"가 아니라 "번개 횟수"로 사용
    - **조치**: 필드명 변경 또는 주석 추가 고려


테스트로 찾은문제
1. 툴팁이 스킬/패시브표시 패널보다 ZIndex 가 작은것같음 뒤에 표시됨
2. 회전하는 스킬의 중심점이 캐릭터가 움직일때 캐릭터의 뒤에서 쪼차오는식으로 보임. 사용자경험 안좋음
3. 번개스킬 비쥬얼라이징 필요함. 
4. 여전히 끝판왕은 몬스터가 유저쫒아오는것 해결하기임.
5. 맵을 만들기는 해야할듯 => 맵을 조밀하게만들고. 물리효과를 좀더 키워야할까.
6. 스킬남발 + 과부하시 protobuf serialize  에러발생.



