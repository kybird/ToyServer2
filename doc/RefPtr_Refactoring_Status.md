# Refactoring Status: Lock-Free Entity Pooling (No Boost)

`std::shared_ptr`의 제어 블록(Control Block) 만료 문제로 인해 발생하는 `std::bad_weak_ptr` 크래시를 근본적으로 해결하고, 잦은 동적 할당 리스크를 피하기 위한 커스텀 참조 카운팅 인터페이스 기반의 락프리 풀링 구조 진행 현황입니다.

---

## 📅 Current Tasks & Progress

- [ ] **Phase 1: Core Smart Pointers**
  - [x] Create `RefCounted.h` (formerly IntrusiveBase) with CRTP and thread-safe reference counting.
  - [x] Create `RefPtr.h` (formerly IntrusivePtr) custom smart pointer.
  - [x] Write single-threaded unit tests for `RefPtr` verifying ref counts and pool callbacks.

- [ ] **Phase 2: Lock-Free Object Pool**
  - [x] Implement `LockFreeObjectPool.h` using `moodycamel::ConcurrentQueue`.
  - [x] Ensure Pop() calls `Reset()` and returns `RefPtr<T>(ptr)`.
  - [x] Write stress tests (multi-threaded Push/Pop) verifying no loose objects.

- [ ] **Phase 3: Framework Integration & Generational ID**
  - [ ] Implement `GenerationalID.h` (Version + Index).
  - [x] Refactor `GameObject` to inherit from `RefCounted<GameObject>`. (in `GameObject.h`)
  - [x] Migrate `PlayerFactory`, `MonsterFactory`, `ProjectileFactory`, `ExpGemFactory` to use `RefPtr` and `LockFreeObjectPool`.
  - [ ] Refactor `Room.h`, `Room.cpp`, and `CombatManager.cpp` managers to use `RefPtr` instead of `std::shared_ptr`.
        *(⚠️ Currently In Progress - Addressing missing `ReturnToPool` and constructor parameter mismatches)*
  - [ ] Use `GenerationalID` handles instead of `weak_ptr` for inter-entity references.
  - [ ] Verify using live monster waves.

---

## 🛠️ Proposed Architecture (Current Plan)

### 1. System/Memory
새로운 커스텀 포인터와 오브젝트 풀 구성을 위한 코어 모듈을 추가하거나 재작성합니다.

#### [NEW] `src/System/Memory/RefPtr.h`
기존 `boost::intrusive_ptr`의 역할을 대체할 수 있는 자체 커스텀 스마트 포인터 기반 시스템입니다. 

- **`::System::RefPtr<T>`**: `boost::intrusive_ptr`을 완벽히 대체하는 커스텀 스마트 포인터 구현 (생성, 복사, 이동, 역참조 연산자 등).
- **`::System::RefCounted<T>`**: CRTP(Curiously Recurring Template Pattern) 형태의 기반 참조 카운트 관리 클래스 추가.
  - 내부에 `std::atomic<uint32_t> m_refCount{0}`을 가짐.
  - `AddRef()`, `Release()` 구현. `Release()` 호출 시 참조 카운트가 0이 되면 풀에 반환하는 콜백 메커니즘 제공.
  - **Memory Order 최적화 (Strict Constraint)**: `AddRef()`는 `std::memory_order_relaxed`를 사용하고, `Release()`는 카운트 감소 시 `std::memory_order_acq_rel`을 사용하여 풀 반환 전 가시성(Visibility)을 철저히 보장합니다.
  - **순환 참조 방지 (Strict Constraint)**: 코어 모듈인 `System/Memory` 내부의 `RefCounted` 계층이 특정 풀 헤더(`LockFreeObjectPool`)를 직접 Include하지 않도록 설계합니다. 참조 카운트 0 도달 시 발생해야 할 풀 반환 행위(`ReturnToPool`)는 CRTP를 활용하거나 정적 콜백(Static Callback) 메커니즘을 팩토리에서 주입받아 분리합니다.

#### [NEW] `src/System/Memory/GenerationalID.h`
- **`GenerationalID`**: `weak_ptr` 대체용 32비트 기반 안전한 핸들 구조체입니다.
- 장기 참조 또는 타겟 지정 등 엔티티 간 교차 참조(예: 특정 몬스터를 쫓는 투사체) 시 포인터 대신 이 핸들을 보관하며, `[상위 16비트: Version + 하위 16비트: 풀 배열 Index]` 형태로 비트 마스킹하여 팩킹/언팩킹하여 외부 ABA 문제를 안전하게 차단합니다.

#### [NEW] `src/System/Memory/LockFreeObjectPool.h`
ABA 문제가 철저하게 방어된 락프리 기반의 객체 풀입니다.

- **풀 내부 ABA 방어 (Updated)**: 기존 네트워크 모듈에서 검증된 `moodycamel::ConcurrentQueue`를 그대로 활용하여 ABA 문제를 해결합니다. (스레드 세이프 및 퍼포먼스 입증형)
- **풀 반환 시 원자적 트랜잭션 (Strict Constraint)**: `Pop()`의 리턴 타입은 절대 Raw Pointer(`T*`)여서는 안 됩니다. 꺼낸 객체를 즉시 `ptr->Reset()`으로 초기화한 후, `::System::RefPtr<T>(ptr)` 형태로 리턴하여 카운트를 원자적으로 1로 올린 상태로 방출합니다.

---

### 2. Entity Framework
오브젝트 생성과 소멸에 관여하는 최상위 오브젝트 및 팩토리 클래스들의 스마트 포인터를 수정합니다.

#### [MODIFY] `src/Examples/VampireSurvivor/Server/Entity/GameObject.h`
모든 게임 오브젝트의 기반 클래스.
- 기존 `std::enable_shared_from_this<GameObject>` 상속을 **제거**했습니다.
- 대신 `::System::RefCounted<GameObject>`를 상속받습니다.

#### [MODIFY] `src/Examples/VampireSurvivor/Server/Entity/PlayerFactory.h`, `MonsterFactory.h`, `ProjectileFactory.h`, `ExpGemFactory.h` (및 `.cpp`)
오브젝트를 스폰(가져오기)하고 릴리즈(반환하기) 하는 주체들입니다.
- 리턴 타입을 `std::shared_ptr<T>`에서 `::System::RefPtr<T>`로 전부 교체합니다.
- 특히 `ExpGem` 객체 역시 수많은 잦은 스폰/디스폰이 발생하므로 규칙 A2(Zero-Allocation) 위반 방지를 위해 무조건 `LockFreeObjectPool`을 사용하는 `ExpGemFactory`를 생성하여 반영합니다.
- `ObjectPool` 대신 향상된 `LockFreeObjectPool`을 사용하도록 교체합니다. 
- **객체 재사용 안전성 강제 (Placement New 금지 및 명시적 Reset 적용 - Strict Constraint)**: 풀에서 꺼낸 객체(`Pop()`)를 재초기화하기 위해 Placement New 활용을 절대적으로 **금지**합니다. (가상 함수 테이블/vtable 훼손 사전 차단). 대신, 명시적 전체 초기화 방식인 `virtual void Reset()`을 호출합니다. 단, 해당 기능 호출 내부에서 기반 클래스(`RefCounted`)의 `m_refCount`와 락프리 노드 멤버 등은 결코 0으로 리셋되지 않도록 주의해야 합니다.

#### [MODIFY] `src/Examples/VampireSurvivor/Server/Game/Room.h`, `Room.cpp`, `CombatManager.h` (및 연관 관리자 리스트들)
- 캐싱하고 있는 플레이어, 몬스터, 투사체 등의 리스트 타입들이 `std::vector<std::shared_ptr<GameObject>>` 형태라면 현존하는 코드를 모두 `::System::RefPtr`로 파싱타입 변경합니다.
- 직접 교차 참조 중인 데이터들을 즉결하여 `weak_ptr` 대체재인 위 `GenerationalID` 구조체를 통해 필요 시점에 매 틱마다 `Room::GetObjectManager().GetObject(target_gen_id)`로 검증을 구하여 접근토록 리팩터링합니다.

---

### 3. 단계별 구현 및 검증 전략 (Incremental Approach)
가장 추천하는 안정적인 도입 순서입니다. 각 단계를 완벽히 검증한 후 다음으로 넘어갑니다.

#### 1단계: 코어 스마트 포인터 구현 (Single-Thread 중심)
- **목표**: `::System::RefPtr<T>`과 `::System::RefCounted<T>` CRTP 클래스 기반 작성.
- **검증**: 멀티스레드 고려 없이 오직 포인터 복사/이동 시 `m_refCount`가 의도대로 정확히 증감하는지, 카운트 0 도달 시 지정된 콜백이 1회 정확히 호출되는지 단위 테스트(Unit Test) 작성 및 통과 확인. (이 때 Memory Order 구현 내용 검토.)

#### 2단계: 락프리 오브젝트 풀 구현 (핵심 난이도)
- **목표**: 1단계에서 완성한 커스텀 포인터와 맞물려 동작하는(순환 참조가 없는) `LockFreeObjectPool<T>` 구현. (기반: `moodycamel::ConcurrentQueue`)
- **검증**: 순수 인터페이스나 더미 구조체(Dummy Struct)를 통한 스트레스 테스트(Stress Test) 작성. 수십 개의 스레드가 동시에 수십만 번의 `Push`/`Pop`을 반복하는 시나리오 구성. 유실 객체, 댕글링 포인터 발생 여부 방어 가능을 검증.

#### 3단계: 게임 프레임워크 연동 및 Generational ID 적용
- **목표**: `GameObject` 상속 구조 변경, 각 팩토리 클래스 포인터 교체 구현, 그리고 `GenerationalID`(`Version + Index` 비트마스킹) 기반 핸들 생성 및 접근 체계(`GetObject()`) 전환 구현.
- **검증**: 전체 기능 구현 후 클라이언트 더미 등을 붙여 극단적인 `WaveManager` 대량 스폰/사망 시나리오 연출. `bad_weak_ptr` 크래시 등 모든 오류 미발생 확인, 소멸 몬스터의 ID를 들고 있던 투사체의 `GetObject(id)` 호출이 좀비 객체를 정확히 걸러내어 안전하게 `nullptr`을 반환하는지 검수 및 `Reset()` 시 락프리 노드 변수 보존 확인.
