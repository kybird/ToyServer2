# 개발 일지 - 2026-01-10 (이동 동기화 고도화)

## 개요
이동 동기화(Dead Reckoning) 및 클라이언트 예측(Client-Side Prediction) 고도화 작업 기록.
테스트 코드와 프로덕션 코드의 분리, 자동화된 검증, 그리고 작업 중 발생하는 주요 의사결정 사항을 기록한다.

## 작업 로그

### [16:53] 작업 시작
- **목표**: Soft Correction 도입, Adaptive Interpolation Delay (with Jitter Buffer) 구현.
- **제약사항**: 
    - 테스트 코드는 반드시 `Assets/Tests` 폴더에 위치하여 프로덕션 빌드에 포함되지 않도록 `ToySurvival.Tests.asmdef`로 격리한다.
    - 유니티 에디터를 직접 실행할 수 없으므로, 코드는 컴파일 에러가 없도록 신중하게 작성한다.

### [16:55] 의사결정: Jitter Buffer 계산 방식
- **문제**: RTT의 표준편차를 구하려면 과거 데이터를 모두 저장해야 하므로 메모리와 연산 비용이 든다.
- **결정**: 이동 평균(Moving Average)과 유사한 방식으로 근사값을 계산하기로 결정.
    - `AvgRTT = AvgRTT * 0.9 + NewRTT * 0.1`
    - `VarRTT = VarRTT * 0.9 + Abs(NewRTT - AvgRTT) * 0.1`
    - `Jitter = VarRTT`
    - 이유: 매 프레임 리스트 순회 없이 O(1)로 처리 가능하며, 게임 틱 동기화에는 충분한 정확도를 가짐.

### [16:57] 의사결정: Soft Correction 보정 속도
- **문제**: 고정 속도로 보정하면 오차가 클 때 너무 오래 걸리고, 작을 때 너무 빠르다.
- **결정**: 지수 함수적 감쇠(Exponential Decay) 사용.
    - `Correction = Error * LerpFactor * dt`
    - 오차가 클수록 빠르게, 작을수록 정밀하게 접근.
    - `LerpFactor`는 10.0f 정도로 시작하여 튜닝.
