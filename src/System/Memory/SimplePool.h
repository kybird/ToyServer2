#pragma once

#include "System/IObjectPool.h"
#include <vector>
#include <mutex>
#include <atomic>

namespace System {

/**
 * @brief Simple mutex-based object pool implementing IObjectPool.
 * 
 * Thread-safe, suitable for game logic where lock overhead is acceptable.
 * For high-performance internal use, see LockFreePool.
 * 
 * @tparam T Type must be default-constructible.
 */
template<typename T>
class SimplePool : public IObjectPool<T> {
public:
    /**
     * @param poolLimit Max idle objects to keep. Excess will be deleted.
     * @param allocLimit Max total allocations. 0 = unlimited.
     */
    SimplePool(size_t poolLimit = 1000, size_t allocLimit = 0)
        : _poolLimit(poolLimit), _allocLimit(allocLimit) {}

    ~SimplePool() {
        std::lock_guard<std::mutex> lock(_mutex);
        for (T* obj : _pool) {
            delete obj;
        }
        _pool.clear();
    }

    T* Acquire() override {
        std::lock_guard<std::mutex> lock(_mutex);
        
        // Try reuse
        if (!_pool.empty()) {
            T* obj = _pool.back();
            _pool.pop_back();
            return obj;
        }

        // Check alloc limit
        if (_allocLimit > 0 && _allocCount >= _allocLimit) {
            return nullptr;
        }

        // New allocation
        ++_allocCount;
        return new T();
    }

    void Release(T* obj) override {
        if (!obj) return;

        // Optional: call Reset() if available
        // if constexpr (requires { obj->Reset(); }) { obj->Reset(); }

        std::lock_guard<std::mutex> lock(_mutex);
        
        if (_pool.size() < _poolLimit) {
            _pool.push_back(obj);
        } else {
            delete obj;
            --_allocCount;
        }
    }

    size_t GetPoolCount() const override {
        std::lock_guard<std::mutex> lock(_mutex);
        return _pool.size();
    }

    size_t GetAllocCount() const override {
        return _allocCount.load();
    }

private:
    mutable std::mutex _mutex;
    std::vector<T*> _pool;
    std::atomic<size_t> _allocCount{0};
    size_t _poolLimit;
    size_t _allocLimit;
};

} // namespace System
