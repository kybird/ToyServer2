#pragma once
#include <atomic>

namespace System {
namespace Debug {

// 성능 측정을 위한 간단한 메모리 메트릭
struct MemoryMetrics {
    // C++17 inline static: 여러 곳에서 include 해도 전역 변수 하나로 합쳐짐
    inline static std::atomic<long long> AllocCount = 0;

    // (옵션) 해제 횟수도 세고 싶다면
    inline static std::atomic<long long> DeallocCount = 0;

    // 현재 사용 중인 메모리 블록 수 (Alloc - Dealloc)
    static long long GetActiveAllocations() { return AllocCount.load() - DeallocCount.load(); }
};

} // namespace Debug
} // namespace System