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
    Join();
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

                    // [Fix] 정지 신호가 오면 즉시 퇴근 (큐에 작업이 남았어도 무시)
                    if (_stop.load(std::memory_order_acquire))
                    {
                        return;
                    }

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
                            LOG_ERROR("Task Worker #{} Unknown Exception!", i);
                        }
                    }
                }
            }
        );
    }
}

void ThreadPool::Stop()
{
    // [Fix] Non-blocking Stop
    if (_stop.exchange(true, std::memory_order_release))
    {
        return; // Already stopped
    }

    // Wake up all threads so they check _stop
    _taskSemaphore.release(_threadCount);
    LOG_INFO("{} Stop signal sent.", _name);
}

void ThreadPool::Join()
{
    // [Fix] Blocking Join
    LOG_INFO("{} Waiting for threads to join...", _name);
    for (auto &t : _threads)
    {
        if (t.joinable())
            t.join();
    }
    _threads.clear();
    LOG_INFO("{} Stopped.", _name);
}

} // namespace System
