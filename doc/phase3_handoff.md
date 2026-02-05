# Phase 3: 성능 최적화 및 잔여 작업 상태 보고서

## 🟢 완료된 작업 (Today)

### 1. 난수 생성기 최적화 (FastRandom)
- `FastRandom.h` (Xorshift128+) 구현 완료.
- `CombatManager`, `WaveManager`, `MonsterFactory`, `AI` (Chaser, Wander, Flocking) 내의 모든 `rand()` 및 `std::mt19937`을 `FastRandom`으로 교체하여 서버 부하 경감.

### 2. 인벤토리 동기화 시스템
- `S_UpdateInventory` 프로토콜 추가.
- 서버측 `Player::SyncInventory()` 구현 및 레벨업 선택 시 강제 동기화 트리거 추가.
- 클라이언트측 `InventoryHUD.cs` 구현으로 현재 보유 스킬/레벨 가독성 확보.

### 3. 클라이언트 시각 피드백 강화
- **HpBar**: 상시 노출에서 데미지 입을 시 3초 노출로 변경 (UI 난잡함 해소).
- **DamageText**: 단순 상승에서 팝업 애니메이션 추가 및 크리티컬 연출 강화.
- **Greatsword**: 부채꼴 공격에 맞게 실제 각도(Arc)를 시각화하는 `AoEUtils.DrawArcAoE` 구현.

### 4. 프로토콜 정리
- 더 이상 사용하지 않는 `S_DebugDrawBox` 관련 코드 및 프로토콜 완전 제거.

---

## 🟡 진행 중 / 남은 작업 (Phase 3 & Next)

### 1. UDP 이동 동기화 (High Priority)
- 현재 이동 패킷은 TCP(S_MoveObjectBatch)를 사용 중.
- 성능 향상을 위해 몬스터 및 플레이어의 위치 정보를 UDP로 전송하는 기능 구현 필요.

### 2. Phase 2 (UI/FX) 마무리
- 클라이언트측 프리팹(Monster, Projectile)의 실제 에셋 적용 및 사운드 효과 통합.
- 현재 인벤토리 아이콘 리소스 로드 로직 주석 해제 및 실제 에셋 연결 필요.

### 3. 코드 정리 (Cleanup)
- `Player.cpp` 및 `Room.cpp` 내에 남아있는 레벨업 관련 임시 하드코딩(특정 스킬 우선 제공 등) 제거.
- `WaveManager`의 몬스터 스폰 밀도 분포(Density Distribution) 알고리즘 추가 최적화.

---

## 💡 참고 사항
- `d:\Project\ToyServer2\src\Examples\VampireSurvivor\Protocol\GenerateProto.bat`를 실행하여 프로토콜 변경 사항이 항상 동기화되도록 유지했습니다.
- 빌드 확인 시 `FastRandom` 도입으로 인한 난수 패턴 변화가 게임 밸런스에 미치는 영향(크리티컬 확률 등)에 대한 모니터링이 필요합니다.
