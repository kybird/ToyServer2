#pragma once
#include <atomic>

namespace System {
namespace Debug {

// [Conditional Compilation]
// Define ENABLE_DIAGNOSTICS to enable hot path counters.
// In Release builds, leave undefined for zero overhead.
// Usage: Add -DENABLE_DIAGNOSTICS to compiler flags or #define before include

// 성능 측정을 위한 간단한 메모리 메트릭
struct MemoryMetrics
{
    // C++17 inline static: 여러 곳에서 include 해도 전역 변수 하나로 합쳐짐
    static std::atomic<long long> AllocCount;
    static std::atomic<long long> DeallocCount;

    // 현재 사용 중인 메모리 블록 수 (Alloc - Dealloc)
    static long long GetActiveAllocations();

    // [Hot Path Diagnostics] 패킷 흐름 추적 카운터
    static std::atomic<long long> RecvPacket; // OnRead에서 파싱된 패킷 수
    static std::atomic<long long> AllocFail;  // AllocatePacket 실패 횟수
    static std::atomic<long long> Posted;     // Dispatcher::Post 성공 횟수
    static std::atomic<long long> Processed;  // Dispatcher::Process 처리 횟수
    static std::atomic<long long> Echoed;     // HandlePacket에서 Echo 응답 횟수
};

} // namespace Debug
} // namespace System