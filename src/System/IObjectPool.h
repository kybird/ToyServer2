#pragma once

namespace System {

/**
 * @brief Generic object pool interface for user-facing pooling.
 * 
 * This interface provides a simple contract for object reuse.
 * Implementations may vary (lock-based, lock-free, etc).
 * 
 * @tparam T Type of objects to pool. Must be default-constructible or
 *           implementation must handle construction.
 */
template<typename T>
class IObjectPool {
public:
    virtual ~IObjectPool() = default;

    /**
     * @brief Acquire an object from the pool.
     * @return Pointer to object, or nullptr if pool exhausted.
     */
    virtual T* Acquire() = 0;

    /**
     * @brief Release an object back to the pool.
     * @param obj Object to return. Must have been acquired from this pool.
     */
    virtual void Release(T* obj) = 0;

    /**
     * @brief Get number of objects currently in pool (idle).
     */
    virtual size_t GetPoolCount() const = 0;

    /**
     * @brief Get total number of objects allocated by this pool.
     */
    virtual size_t GetAllocCount() const = 0;
};

} // namespace System
