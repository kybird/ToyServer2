#pragma once
#include <atomic>
#include <cstdint>

namespace System {

/**
 * @brief CRTP Base class for intrusive reference counting.
 *
 * Requirements:
 * 1. m_refCount must be atomic.
 * 2. AddRef uses memory_order_relaxed for performance.
 * 3. Release uses memory_order_acq_rel for visibility.
 * 4. Must NOT include LockFreeObjectPool.h or specific pool headers (Circular Dependency Prevention).
 *    Instead, derived classes must implement `void ReturnToPool() const`.
 */
template <typename T> class RefCounted
{
public:
    RefCounted() = default;
    virtual ~RefCounted() = default;

    // Prevent copying/moving
    RefCounted(const RefCounted &) = delete;
    RefCounted &operator=(const RefCounted &) = delete;
    RefCounted(RefCounted &&) = delete;
    RefCounted &operator=(RefCounted &&) = delete;

    void AddRef() const
    {
        // Relaxed order because we only care about incrementing the counter atomically.
        // No memory synchronization is needed when just taking a reference.
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    void Release() const
    {
        // acq_rel ensures that all memory operations before the last Release()
        // are visible before the object is returned to the pool (or deleted).
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            // CRTP Callback: The derived class is responsible for sending itself back to the pool.
            // This strictly breaks the dependency between the Memory module and the Pool module.
            // Cast away constness here because ReturnToPool typically modifies or deletes the object.
            const_cast<T *>(static_cast<const T *>(this))->ReturnToPool();
        }
    }

    uint32_t GetRefCount() const
    {
        return m_refCount.load(std::memory_order_relaxed);
    }

protected:
    mutable std::atomic<uint32_t> m_refCount{0};
};

} // namespace System
