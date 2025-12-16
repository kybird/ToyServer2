#include "System/Thread/ThreadPool.h"
#include "System/ILog.h"
#include "System/Pch.h"

namespace System {

ThreadPool::ThreadPool(int threadCount) : _threadCount(threadCount), _running(false), _stop(false)
{
}

ThreadPool::~ThreadPool()
{
    Stop(false);
}

void ThreadPool::Start()
{
    if (_running)
        return;

    _running = true;
    _stop = false;

    LOG_INFO("ThreadPool (Task) Starting with {} threads...", _threadCount);
    _threads.reserve(_threadCount);

    for (int i = 0; i < _threadCount; ++i)
    {
        _threads.emplace_back(
            [this, i]()
            {
                // LOG_INFO("Task Worker #{} Started", i); // Optional spam reduction
                while (true)
                {
                    std::function<void()> task;

                    // Try dequeue
                    if (_tasks.try_dequeue(task))
                    {
                        try
                        {
                            task();
                        } catch (const std::exception &e)
                        {
                            LOG_ERROR("Task Worker #{} Exception: {}", i, e.what());
                        }
                    }
                    else
                    {
                        // Check stop condition ONLY when queue is empty (or check generic stop
                        // flag)
                        if (_stop)
                        {
                            // FINISH_PENDING: _stop is true, but we are in the else branch checking
                            // isEmpty. If queue empty and stop true -> Exit.
                            // Double check emptiness to be sure
                            if (_tasks.size_approx() == 0)
                                break;
                            // Otherwise loop again to drain
                            std::this_thread::yield();
                            continue;
                        }

                        // Idle strategy: Yield to avoid spinning 100% CPU
                        // In a high-perf scenario with 'BlockingConcurrentQueue', we could
                        // block. Here we use yield/sleep hybrid for simplicity with
                        // ConcurrentQueue.
                        std::this_thread::yield();
                        // std::this_thread::sleep_for(std::chrono::microseconds(1));
                    }
                }
                // LOG_INFO("Task Worker #{} Stopped", i);
            }
        );
    }
}

void ThreadPool::Stop(bool wait)
{
    if (!_running)
        return;

    // Set stop flag
    _stop = true;

    // If not waiting, we might want to clear the queue, but ConcurrentQueue
    // doesn't have a clear(). We just let the workers exit.
    // If wait=true, workers loop until empty.
    // If wait=false, we rely on checking _stop.

    // However, the worker loop logic above prioritizes finishing tasks if they exist in queue.
    // To truly ABORT (wait=false), we would need a stronger check inside the dequeue success block,
    // but that risks dropping a dequeued task.
    // For this implementation, we simply signal stop.
    // The workers will drain the queue then exit.
    // If we really wanted drastic abort, we'd need to detach or cancel futures (not easy in C++).

    // Join threads
    for (auto &t : _threads)
    {
        if (t.joinable())
            t.join();
    }
    _threads.clear();
    _running = false;
    LOG_INFO("ThreadPool Stopped. (Wait={})", wait);
}

} // namespace System
