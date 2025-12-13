#pragma once

#include "System/Pch.h"
#include <atomic>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <vector>


namespace System {

class Packet;
void intrusive_ptr_add_ref(Packet *p);
void intrusive_ptr_release(Packet *p);

class Packet
{
public:
    Packet(size_t initialCapacity = 4096) : _refCount(0)
    {
        _buffer.reserve(initialCapacity);
    }

    // Disable Copy/Move to prevent accidental heavy copies
    Packet(const Packet &) = delete;
    Packet &operator=(const Packet &) = delete;
    Packet(Packet &&) = delete;
    Packet &operator=(Packet &&) = delete;

    ~Packet() = default;

    // Direct buffer access
    uint8_t *data()
    {
        return _buffer.data();
    }
    const uint8_t *data() const
    {
        return _buffer.data();
    }

    size_t size() const
    {
        return _buffer.size();
    }
    size_t capacity() const
    {
        return _buffer.capacity();
    }

    // vector-like interface
    void reserve(size_t newCapacity)
    {
        _buffer.reserve(newCapacity);
    }

    void resize(size_t newSize)
    {
        // Optimization: For now, we use standard resize (zero-init).
        // Future: Use uninitialized_value_construct or custom allocator if needed.
        _buffer.resize(newSize);
    }

    void clear()
    {
        _buffer.clear();
    }

    void assign(const uint8_t *begin, const uint8_t *end)
    {
        _buffer.assign(begin, end);
    }

    template <typename InputIt> void assign(InputIt first, InputIt last)
    {
        _buffer.assign(first, last);
    }

    // Called by PacketPool when retrieving from pool
    void Reset()
    {
        _buffer.clear();
        // Crucial: Ref count must be 0 when coming out of pool
        _refCount.store(0, std::memory_order_relaxed);
    }

private:
    friend void intrusive_ptr_add_ref(Packet *p);
    friend void intrusive_ptr_release(Packet *p);

    std::atomic<int> _refCount;
    std::vector<uint8_t> _buffer;
};

// Inline add_ref for performance
inline void intrusive_ptr_add_ref(Packet *p)
{
    // relaxed order is sufficient for increment
    p->_refCount.fetch_add(1, std::memory_order_relaxed);
}

// release is defined in .cpp to avoid circular dependency with PacketPool
void intrusive_ptr_release(Packet *p);

} // namespace System
