#include "System/Debug/MemoryMetrics.h"

namespace System {
namespace Debug {

std::atomic<long long> MemoryMetrics::AllocCount = 0;
std::atomic<long long> MemoryMetrics::DeallocCount = 0;

long long MemoryMetrics::GetActiveAllocations()
{
    return AllocCount.load() - DeallocCount.load();
}

std::atomic<long long> MemoryMetrics::RecvPacket = 0;
std::atomic<long long> MemoryMetrics::AllocFail = 0;
std::atomic<long long> MemoryMetrics::Posted = 0;
std::atomic<long long> MemoryMetrics::Processed = 0;
std::atomic<long long> MemoryMetrics::Echoed = 0;

} // namespace Debug
} // namespace System
