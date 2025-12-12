#include "System/Memory/PacketPool.h"

namespace System {

// [핵심] 힙에 할당하고 절대 delete 하지 않음 (Leaky Pointer)
// 이렇게 하면 main()이 끝나도 소멸자가 호출되지 않아서 크래시가 안 남
moodycamel::ConcurrentQueue<std::vector<uint8_t> *> *PacketPool::_pool =
    new moodycamel::ConcurrentQueue<std::vector<uint8_t> *>();

std::atomic<int> PacketPool::_poolSize = 0;

} // namespace System