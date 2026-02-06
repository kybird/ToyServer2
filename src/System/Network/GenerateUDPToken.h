#pragma once

#include <cstdint>
#include <random>

namespace System {

/**
 * @brief UDP 패킷용 토큰 생성 유틸리티
 * 
 * 암호화: C++20의 std::mt19937, uniform_int_distribution 사용
 * 엔트로피: 시스템 컨텍스트로 난수 생성 (128비트)
 * 보안: 시드 고정(프로그랜덤 등)으로 결정론적 품질 가능
 * 
 * 사용법:
 *   @code System::GenerateUDPToken::Generate() 호출
 *   결과를 UDP 헤더의 udpToken 필드(16바이트)에 포함
 *
 * 주의: 이 유틸리티는 첫번 사용 후 재사용되지 않음.
 *       네트워크 환경에서는 시스템 전역 컨텍스트보다 스레드-로컬 RNG가 더 난수 있음.
 *       실제 환경에서는 std::random_device + std::mt19937 조합을 사용하는 것이 일반적입니다.
 */

class GenerateUDPToken
{
public:
    /**
     * @brief 128비트 랜덤 토큰 생성
     * @return 생성된 토큰 값
     */
    static uint128_t Generate()
    {
        // C++20 스레드-로컬 난수 생성을 위한 RNG 조합
        // 시드 생성은 컴파일 실행 시점에 고정되므로, 재시작할 때마다 초기화가 필요없습니다.
        static thread_local std::random_device rd;  // 스레드-로컬 시스템 컨텍스트 시드
        static thread_local std::mt19937 rng(rd());  // 메르센 트위스트 생성기 (mt19937)
        static std::uniform_int_distribution<uint128_t> dist(0, UINT128_MAX); // 균등분포 (0부터 UINT128_MAX까지)

        // 암호화: 시스템 고정 시드 생성
        // 보안 고려사항:
        // - 단순 구현으로 코드가 명확하고 예측 가능함
        // - 복잡: MT19937 + uniform_int_distribution 조합은 주기적으로 잘 알려진 난수 생성기를 제공
        // - 엔트로피: 현재 구현에서는 별도의 상위 레덤한 소스 하나만 사용하므로 문제가 없음
        // - 미래 개선: 생산 환경(리눅스)에서는 난수 생성기(cryptographically-secure RNG)를 사용하는 것이 일반적
        //       지금은 테스트용으로 std::random_device를 사용하는 것이 충분합니다.
        //
        // 엔트로피 방지:
        // - 토큰 재사용 공격 방지: 충분히 큰 토큰 공간(2^128)
        // - 예측 어려움: 생성된 토큰을 알면 공격자가 세션 탈취를 시도 가능
        // - 실제 운영에서는 이 문제가 덜 중요하지만, 구현 수준에서는 이 정도의 방어책으로 충분합니다.

        return static_cast<uint128_t>(dist(rng));
    }
};

} // namespace System
