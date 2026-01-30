#include "System/Thread/ThreadPool.h"
#include "System/ILog.h"
#include "System/Pch.h"

namespace System {

ThreadPool::ThreadPool(int threadCount, const std::string &name) : _threadCount(threadCount), _name(name), _stop(false)
{
    if (threadCount <= 0)
    {
        throw std::invalid_argument(
            "ThreadPool thread count must be positive. Received: " + std::to_string(threadCount)
        );
    }
}

ThreadPool::~ThreadPool()
{
    Stop();
}

void ThreadPool::Start()
{
    if (!_threads.empty())
        return;

    _stop = false;

    LOG_INFO("{} Starting with {} threads...", _name, _threadCount);
    _threads.reserve(_threadCount);

    for (int i = 0; i < _threadCount; ++i)
    {
        _threads.emplace_back(
            [this, i]()
            {
                // LOG_INFO("Task Worker #{} Started", i); // Optional spam reduction
                while (true)
                {
                    _taskSemaphore.acquire();

                    std::function<void()> task;
                    if (_tasks.try_dequeue(task))
                    {
                        try
                        {
                            task();
                        } catch (const std::exception &e)
                        {
                            LOG_ERROR("Task Worker #{} Std Exception: {}", i, e.what());
                        } catch (...)
                        {
                            LOG_ERROR("Task Worker #{} Unknown Exception (SEH/Non-Std)!", i);
                        }
                        continue;
                    }

                    // Only return if acquired but no task (empty) AND stopped.
                    if (_stop.load(std::memory_order_acquire))
                    {
                        return;
                    }
                }
            }
        );
    }
}

void ThreadPool::Stop()
{
    // Set stop flag
    _stop.store(true, std::memory_order_release);

    // Wake up all threads so they check _stop
    _taskSemaphore.release(_threadCount);

    // Join threads
    for (auto &t : _threads)
    {
        if (t.joinable())
            t.join();
    }
    _threads.clear();
    LOG_INFO("{} Stopped.", _name);
}

} // namespace System
