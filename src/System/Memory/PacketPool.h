#pragma once

#include "System/Pch.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <vector>

namespace System {

/*
    Pools std::vector<uint8_t> to reduce allocations for variable length
   packets. The capacity of vectors in the pool is preserved, allowing reuse for
   large packets.
*/
class PacketPool {
public:
  static std::shared_ptr<std::vector<uint8_t>> Allocate(size_t size) {
    std::vector<uint8_t> *ptr = nullptr;
    if (_pool.try_dequeue(ptr)) {
      // Reuse existing vector
      ptr->clear();
      if (ptr->capacity() < size) {
        ptr->reserve(size);
      }
    } else {
      // Create new vector
      ptr = new std::vector<uint8_t>();
      ptr->reserve(size);
    }

    // Return with custom deleter that returns to pool
    return std::shared_ptr<std::vector<uint8_t>>(
        ptr, [](std::vector<uint8_t> *p) { Release(p); });
  }

private:
  static void Release(std::vector<uint8_t> *ptr) { _pool.enqueue(ptr); }

  static moodycamel::ConcurrentQueue<std::vector<uint8_t> *> _pool;
};

// Define static member in header for simplicity (inline) or need cpp file?
// Template static members are fine in headers, but this is a normal class.
// We should put the definition in a cpp file or make it inline/template.
// To avoid strict ODR issues, let's just make it a CP file or use inline
// variable (C++17).

} // namespace System
