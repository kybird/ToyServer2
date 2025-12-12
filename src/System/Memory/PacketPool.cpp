#include "System/Memory/PacketPool.h"

namespace System {

moodycamel::ConcurrentQueue<std::vector<uint8_t> *> PacketPool::_pool;

} // namespace System
