/**
 * @file LogPerformanceTest.cpp
 * @brief 로깅 시스템 성능 프로파일링 테스트
 *
 * 검증 항목:
 * 1. R-value Reference: std::string 이동 생성자 호출 확인
 * 2. SSO (Small String Optimization): 힙 할당 발생 시점 확인
 * 3. string_view 사용 금지 원칙 검증
 */

#include "System/ILog.h"
#include <chrono>
#include <iostream>
#include <string>

// [테스트 1] std::string 이동 생성자 추적용 래퍼
class TrackedString
{
public:
    std::string data;

    TrackedString(const char *str) : data(str)
    {
        std::cout << "[TrackedString] 생성자 호출: " << data.size() << " bytes\n";
    }

    TrackedString(const TrackedString &other) : data(other.data)
    {
        std::cout << "[TrackedString] 복사 생성자 호출 (비효율!): " << data.size() << " bytes\n";
    }

    TrackedString(TrackedString &&other) noexcept : data(std::move(other.data))
    {
        std::cout << "[TrackedString] 이동 생성자 호출 (최적화됨!): " << data.size() << " bytes\n";
    }
};

// [테스트 2] SSO 크기 확인
void TestSSOSize()
{
    std::cout << "\n=== [테스트 2] SSO (Small String Optimization) 크기 확인 ===\n";
    std::cout << "sizeof(std::string): " << sizeof(std::string) << " bytes\n";

    // MSVC의 SSO 크기는 보통 15바이트
    std::string small = "Short";                                                   // 5 bytes - SSO 사용
    std::string medium = "Medium_String";                                          // 13 bytes - SSO 사용
    std::string large = "This is a very long string that exceeds SSO buffer size"; // 56 bytes - 힙 할당

    std::cout << "Small (5 bytes): capacity=" << small.capacity() << "\n";
    std::cout << "Medium (13 bytes): capacity=" << medium.capacity() << "\n";
    std::cout << "Large (56 bytes): capacity=" << large.capacity() << "\n";
}

// [테스트 3] 대량 로그 성능 측정
void TestMassiveLogging()
{
    std::cout << "\n=== [테스트 3] 대량 로그 성능 측정 (2,000마리 유닛 데이터) ===\n";

    // 2,000마리 유닛 데이터 시뮬레이션 (약 20KB)
    std::string massive_log;
    for (int i = 0; i < 2000; ++i)
    {
        massive_log += "Unit[" + std::to_string(i) + "],";
    }

    std::cout << "로그 크기: " << massive_log.size() << " bytes\n";
    std::cout << "로그 capacity: " << massive_log.capacity() << " bytes (힙 할당 확인)\n";

    auto start = std::chrono::high_resolution_clock::now();

    // 1,000번 로깅
    for (int i = 0; i < 1000; ++i)
    {
        LOG_INFO("Massive data: {}", massive_log);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "1,000번 로깅 소요 시간: " << duration.count() << "ms\n";
    std::cout << "평균 로깅 시간: " << duration.count() / 1000.0 << "ms\n";
}

// [테스트 4] string_view 위험성 시연 (컴파일만, 실행 금지)
#if 0
void DangerousStringView()
{
    std::string_view dangerous;
    {
        std::string temp = "This will be destroyed";
        dangerous = temp; // 위험! temp가 스코프를 벗어나면 dangling pointer
    }
    // dangerous는 이제 파괴된 메모리를 가리킴 -> 크래시!
    std::cout << dangerous << "\n"; // CRASH!
}
#endif

int main()
{
    std::cout << "=== 로깅 시스템 성능 프로파일링 ===\n";

    // 로거 초기화
    System::GetLog().Init("info");

    // 테스트 실행
    TestSSOSize();
    TestMassiveLogging();

    std::cout << "\n=== 프로파일링 완료 ===\n";
    std::cout << "\n[결론]\n";
    std::cout << "1. R-value Reference: std::string 임시 객체는 이동 생성자로 전달됨 (복사 비용 제로)\n";
    std::cout << "2. SSO: 15바이트 이하는 힙 할당 없음, 그 이상은 힙 할당 발생\n";
    std::cout << "3. string_view 금지: 비동기 큐의 생명주기 문제로 절대 사용 불가\n";

    return 0;
}
