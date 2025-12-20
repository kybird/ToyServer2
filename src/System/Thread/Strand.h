#pragma once

#include "System/Thread/IStrand.h"
#include "System/Thread/ThreadPool.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

namespace System {

class Strand : public IStrand, public std::enable_shared_from_this<Strand>
{
public:
    Strand(std::shared_ptr<ThreadPool> threadPool) : _threadPool(threadPool), _isScheduled(false)
    {
    }

    void Post(std::function<void()> task) override
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(task));
        }
        Schedule();
    }

private:
    void Schedule()
    {
        // Try to transition from Not Scheduled -> Scheduled
        bool expected = false;
        if (_isScheduled.compare_exchange_strong(expected, true))
        {
            // Successfully became the scheduler
            if (auto pool = _threadPool.lock())
            {
                pool->Enqueue(
                    [self = shared_from_this()]()
                    {
                        self->Run();
                    }
                );
            }
        }
    }

    void Run()
    {
        while (true)
        {
            std::function<void()> task;

            // Critical Section: Fetch Task
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_queue.empty())
                {
                    // No more tasks, release schedule
                    _isScheduled = false;
                    return;
                }
                task = std::move(_queue.front());
                _queue.pop();
            }

            // Execute outside lock
            if (task)
            {
                task();
            }
        }
    }

private:
    std::weak_ptr<ThreadPool>
        _threadPool; // Weak to prevent cycle (ThreadPool -> (Task capture Strand) -> ThreadPool) ??
    // actually ThreadPool doesn't own Strand. Framework owns Strand. Framework owns ThreadPool.
    // ThreadPool owns generic tasks.
    // If Strand owns ThreadPool shared_ptr, and ThreadPool has a task capturing Strand shared_ptr...
    // Cycle: Strand -> ThreadPool -> Task -> Strand.
    // YES, Cycle possible if Task sits in queue.
    // Using weak_ptr is safer for the pool reference.

    std::mutex _mutex;
    std::queue<std::function<void()>> _queue;
    std::atomic<bool> _isScheduled;
};

} // namespace System
