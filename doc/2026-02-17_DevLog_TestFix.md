# 2026-02-17 DevLog: Test Fix & Projectile Logic Correction

## 개요
테스트 실행 시 무한 대기(Hang) 현상과 특정 테스트(`CombatTest`, `EffectSystemTest`) 실패 원인을 분석하고, 이를 수정함. 특히 **유닛 테스트 환경에서** 발생한 `SkillInfo` 구조체 초기화 누락 문제와 투사체 로직을 개선함.

## 주요 변경 사항

### 1. [Test Issue] SkillInfo 구조체 초기화 안전성 강화
- **파일**: `Server/Core/DataManager.h`
- **문제**: **유닛 테스트(`TestCombat.cpp`)에서** `SkillInfo` 구조체를 직접 생성하여 사용할 때, 일부 멤버 변수(pierce, damage 등)를 초기화하지 않아 쓰레기 값(Garbage Value)이 들어가는 문제 발생. (실제 서버 런타임은 JSON 로더가 값을 채우므로 영향 없음)
- **증상**: 테스트 실행 시 `pierce` 값이 비정상적으로 커져 몬스터가 즉사(Overkill)하거나 무한 루프에 가까운 연산을 수행.
- **해결**: `SkillInfo` 구조체의 모든 멤버 변수에 기본값(Default Member Initializer)을 할당하여, 테스트 코드 등에서 직접 생성 시에도 안전한 값을 가지도록 개선.

### 2. [Feature] 투사체(Projectile) 중복 타격 방지 로직 추가
- **파일**: `Server/Game/CombatManager.cpp`, `Server/Entity/Projectile.h`
- **문제**: 관통(Pierce) 속성을 가진 투사체가 몬스터와 충돌할 때, HitBox가 겹쳐 있는 동안(약 0.1~0.2초) 매 프레임 충돌 처리가 발생하여 의도치 않은 다단 히트(Multi-hit)가 일어남.
- **해결**:
    - `Projectile` 클래스에 `_hitTargets` 목록 및 `HasHit()`, `AddHit()` 메서드 추가.
    - `CombatManager::ResolveProjectileCollisions`에서 충돌 처리 시 `HasHit` 여부를 검사하여 이미 타격한 대상은 무시하도록 로직 추가.
    - 객체 풀링(SimplePool) 사용 시 `Reset` 메서드에서 히트 리스트를 초기화(`clear`)하여 재사용 안전성 확보.

### 3. [Test] 유닛 테스트 로직 보완
- **파일**: `tests/TestEffectSystem.cpp`
    - `KnockbackStateBlocksAI`: 몬스터 생성 후 `ObjectManager`에 등록하지 않아 상태 업데이트(`Update`)가 돌지 않던 오류 수정.
- **파일**: `tests/TestCombat.cpp`
    - `LinearEmitterHitsNearestMonster`: 시뮬레이션 시간(0.6s)이 스킬 쿨타임(0.5s)보다 길어 2회 발사되는 현상을 수정. 시간을 0.4s로 단축하여 단일 타격 검증으로 변경.

### 4. [Env] 테스트 환경 점검
- **증상**: `RunTests.bat` 실행 시 최신 코드가 빌드되지 않고 과거 바이너리로 실행되는 문제 확인.
- **조치**: 수동으로 `build.bat` 실행 후 테스트 수행. 향후 `RunTests.bat`에 빌드 단계 포함 권장.

### 5. [Refactor] MockSystem.h 위치 이동
- **파일**: `src/System/MockSystem.h` -> `src/Examples/VampireSurvivor/tests/MockSystem.h`
- **문제**: 테스트용 목(Mock) 클래스가 시스템 코어 디렉토리(`System`)에 포함되어 있어 구조적으로 부적절함.
- **해결**: 해당 파일을 테스트 디렉토리로 이동하고, 관련 테스트 코드들의 Include 경로를 수정함.

## 결과
- `UnitTests.exe`의 63개 테스트 모두 통과 (**PASSED**).
- 관통형 투사체의 동작 안전성 확보.
- 테스트 신뢰성 향상.
