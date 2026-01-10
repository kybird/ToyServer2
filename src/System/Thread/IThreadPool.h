#pragma once
#include <functional>

namespace System {

/**
 * @brief Thread Pool Interface
 */
class IThreadPool
{
public:
    virtual ~IThreadPool() = default;

    // Start workers
    virtual void Start() = 0;

    // Stop workers and drain queue
    virtual void Stop() = 0;

    // Submit a task to the queue
    virtual void Submit(std::function<void()> task) = 0;

    // Get the number of threads in the pool
    virtual size_t GetThreadCount() const = 0;
};

} // namespace System
