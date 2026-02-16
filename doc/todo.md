# Project TODO List

## 🚀 Tasks In-Progress & Remaining

### Next Sprint Planning
- [ ] **네트워크 패킷 처리 구조 근본적 개선**
    - 현재 서버(10KB) 및 스트레스 테스트 클라이언트(8KB)에 하드코딩된 패킷 상한선 존재 (설계 미스).
    - 수신 상황에 따른 **동적 수신 버퍼 확장 구조** 도입 필요.
    - PacketHeader의 SizeType(uint16_t)이 허용하는 최대치(64KB)까지 유연하게 대응하도록 개편.

---

## ✅ Completed Tasks (Moved to DevLogs)
*관련 내용은 doc/YYYY-MM-DD_DevLog.md 파일을 참조하십시오.*


1. 박쥐가 경험치 보석뒤에표시됨
2. 박쥐가 데미지 받았을때 좀비가 빨간색이 되는것처럼 박쥐도 그랬으면 좋겠음
5. 주변을 돌아가는 이펙트가 생기지만 효과없음 마늘?
8. 경험치바가 줄었다 늘었다가
9. HP 관련카드 선택했을때 HP바 바로반영안됨
12. 채찍 이펙트가 원형이다 직사각형이고 레벨업될수록 길어지게
13. 성서 스킬기능이 불명확함 정확한기능은뭐냐?


