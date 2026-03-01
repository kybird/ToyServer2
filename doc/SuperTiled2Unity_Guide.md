# SuperTiled2Unity 세팅 가이드 (Unity용 Tiled 맵 연동)

이 문서는 Tiled 에디터에서 제작한 맵 파일(.tmx)을 Unity 프로젝트(클라이언트) 환경으로 완벽하게 가져오기 위한 **SuperTiled2Unity** 플러그인의 설치 및 설정 방법을 안내합니다.

## 1. 개요
SuperTiled2Unity는 Tiled Map Editor의 데이터를 Unity의 네이티브 2D 타일맵 시스템, 충돌체(Collider), 정렬 레이어 등으로 변환해주는 강력하고 권장되는 오픈 소스 도구입니다. 서버의 C++ JSON 파싱 물리 판정 구조와 클라이언트의 시각적 렌더링 형태를 완벽히 일치시키기 위해 사용합니다.

## 2. SuperTiled2Unity 설치 방법

1. **에셋 다운로드**
   * SuperTiled2Unity 공식 GitHub 또는 itch.io 다운로드 페이지에 접속합니다.
   * 최신 버전의 **`SuperTiled2Unity.unitypackage`** 파일을 다운로드합니다.
   * 공식 사이트: [https://seanba.com/SuperTiled2Unity](https://seanba.com/SuperTiled2Unity)
   * Github: [https://github.com/Seanba/SuperTiled2Unity](https://github.com/Seanba/SuperTiled2Unity)

2. **Unity 프로젝트에 패키지 임포트**
   * Unity 에디터를 실행한 상태에서 다운로드 받은 `SuperTiled2Unity.unitypackage` 파일을 더블클릭합니다.
   * `Import Unity Package` 창이 뜨면 모든 항목이 선택된 것을 확인한 뒤 **[Import]** 버튼을 클릭합니다.
   * 프로젝트 뷰(Project View)에 `SuperTiled2Unity` 폴더가 생성되었는지 확인합니다.

---

## 3. Tiled 맵 작성 시 서버-클라이언트 규칙

서버(ToyServer2)의 맵 처리 기준과 맞추기 위해 Tiled 에디터에서 아래 규칙을 지켜서 맵을 작성해야 합니다.

1. **저장 및 통합 권장 (JSON vs TMX)**
   * **서버용 (C++)**: Tiled 에디터에서 `파일(File) -> 내보내기(Export)`를 눌러 `.json` 형식으로 내보낸 뒤 서버의 `data/` 폴더(`Map01.json`)에 넣어 물리를 적용합니다.
   * **클라이언트용 (Unity)**: Tiled 에디터의 원본 **`.tmx` 파일과 맵 구성에 사용된 타일셋 이미지 파일(.png)**을 Unity 프로젝트 폴더로 드래그 앤 드롭합니다. 

2. **충돌 처리 기준점 (Layer Name)**
   * Tiled 내 타일 레이어를 만들 때, **이름을 `Collision` 또는 `collision`**으로 설정해야만 서버가 100% 벽(Blocked)으로 인식합니다.
   * SuperTiled2Unity는 맵을 가져올 때 이 레이어도 그대로 가져오므로, 클라이언트 측 로직에서도 필요 시 이 레이어를 식별하기 편합니다.

3. **좌표축 (맵 설정 매칭)**
   * Unity는 기본적으로 화면 정중앙이 `(0, 0)`이고 위로 축이 증가(`y축 ↑`)하지만, 
   * 최신 서버 엔진 코드 기준, **Top-Left 기준 (좌상단이 (0, 0), 우하단으로 갈수록 좌표가 커지는 +Y Down 방식)**으로 이동 좌표계가 렌더링되도록, Tiled Map 설정과 Unity 카메라 설정(Orthographic)을 잘 매칭해야 합니다.

---

## 4. Unity로 맵 드래그 앤 드롭 (Import Process)

1. Tiled(.tmx) 파일과 타일 스프라이트 파일(.png)들을 Unity 프로젝트 뷰 하위 폴더(예: `Assets/Maps`)에 드래그하여 임포트합니다.
   * **주의사항**: 반드시 `*.tmx` 파일과 타일셋 관련 이미지 파일들이 **모두 Unity 프로젝트 내부로 들어와야** 플러그인이 정상 작동합니다.

2. SuperTiled2Unity가 `.tmx` 파일을 인식하면 자동으로 Unity 커스텀 에셋 아이콘으로 표시하면서, 내부적으로 Unity의 프리팹(Prefab) 데이터로 자동 반환합니다.

3. 임포트가 끝난 해당 타일맵 에셋(tmx 파일 아이콘)을 클릭해서 Hierarchy(씬 뷰)로 끌어다 놓습니다. 
   * 클릭 시 인스펙터 창에서 `Custom Import Settings`를 확인할 수 있는데 기본적으로 "Pixels Per Unit(PPU)" 수치가 **프로젝트에서 사용하는 픽셀 단위(보통 16, 32, 100 등)와 이미지들의 PPU 세팅을 맞추어** 스케일 문제가 생기지 않도록 설정해야 합니다 (타일 한 개의 사이즈 값과 맞추는 것이 이상적).

4. Hierarchy에 올라가면 맵이 그려지며, `Grid` 하위에 Tiled에서 만들었던 레이어들이 전부 `Tilemap` 오브젝트로 깔끔하게 구성된 것을 보실 수 있습니다.

---

## 5. 추가 설정 힌트 (콜라이더 등)

* Tiled 에디터에서 특정 레이어(Collision 등)나 각 타일 이미지(Tileset Editor) 마다 Tiled의 오리지널 기능인 `Object Layer` (충돌 폴리곤)를 추가해두면, SuperTiled2Unity가 로드 시 자동으로 `Tilemap Collider 2D` 와 `Composite Collider 2D` 컴포넌트를 부착하여 클라이언트 측에서의 시각적 벽 처리/디버깅에도 활용할 수 있습니다.
* 현재 프로젝트 서버에서는 C++이 직접 맵 배열 기반의 CCD(연속 충돌 검사) 충돌과 슬라이딩 계산(SweepTest)을 완벽하게 주관하므로, 클라이언트 단위에서 굳이 Rigidbody Collider 연산을 섞어 쓸 필요 없이 **시각적인 렌더링과 레이어 뎁스(Sorting 레이어) 지정이 주 목적**이 됩니다.
