# 🎒 인벤토리 UI 자동화 작업 계획서 (Inventory Automation Plan)

이 문서는 인벤토리 HUD가 인스펙터 수동 수립 없이도 리소스를 자동으로 로드하고 UI를 생성하도록 개선하기 위한 작업 가이드입니다.

---

## 📅 작업 개요
- **목표**: 인벤토리 아이템 표시 기능의 런타임 자동화 및 리소스 연결 안정화
- **참조 문서**: `doc/RESOURCE_GUIDE_Vampire.md`
- **대상 파일**:
  - `InventoryHUD.cs`: 리소스 로드 및 아이콘 생성 로직 개선
  - `GameSceneInitializer.cs`: 씬 진입 시 인벤토리 UI 존재 여부 확인 및 자동 생성

---

## 🛠️ 상세 작업 목록

### 1. InventoryHUD.cs 기능 고도화 [C1]
- [ ] **싱글톤 및 자동 생성**: `Instance` 접근 시 씬에 없으면 자동으로 루트 Canvas를 찾아 자기 자신을 포함한 UI 구조를 동적으로 생성하는 기능 검토.
- [ ] **리소스 자동 로드**:
  - `_iconPrefab`을 인스펙터에서 넣지 않아도 `Resources.Load<Image>("Prefabs/InventoryIcon")`를 통해 자동으로 가져오도록 수정.
- [ ] **컨테이너 자동 탐색**:
  - `Awake()` 시점에 `transform.Find()` 등을 사용하여 `WeaponContainer`, `PassiveContainer` 자식 오브젝트를 자동으로 찾아 연결.
- [ ] **아이콘 로드 경로 수정**:
  - `GetIconSprite()` 함수 내부의 `Resources.Load` 경로를 `doc/RESOURCE_GUIDE_Vampire.md`에 명시된 `Textures/{iconName}` 경로와 일치하도록 최종 수정.

### 2. GameSceneInitializer.cs 연동 [C2]
- [ ] **UI 매니저 체크**: `GridVisualizer` 생성 로직과 유사하게, 씬 초기화 시 `InventoryHUD` 인스턴스가 없으면 전용 프리팹(`Prefabs/InventoryHUD`)을 로드하여 생성하는 로직 추가.

### 3. 리소스 가이드 준수 확인 [C3]
- [ ] `Assets/_Project/Resources/Textures/` 내에 가이드에 명시된 `dagger.png`, `Icon_Exp.png` 등의 리소스가 실제로 존재하는지 `Debug.LogWarning`을 통해 리포트하는 기능 강화.

---

## 🧪 검증 및 테스트 방법
1. **유니티 에디터 실행**: GameScene 로드 시 Hierarchy에 `InventoryHUD` 오브젝트가 자동 생성되는지 확인.
2. **서버 연동 테스트**: 경험치 보석 획득을 통해 레벨업 시 서버로부터 `S_UpdateInventory` 패킷을 받고 아이콘이 정상적으로 나타나는지 확인.
3. **로그 확인**: 아이콘 리소스 로드 실패 시 `[InventoryHUD] 리소스 로드 실패: ...` 경고 로그가 발생하는지 모니터링.

---

## 🚀 다음 세션 실행 가이드
새 세션을 시작한 후 에이전트에게 다음과 같이 요청하세요:
> "doc/PLAN_Inventory_Automation.md 계획서를 기반으로 /unity-expert 작업을 진행해줘."
