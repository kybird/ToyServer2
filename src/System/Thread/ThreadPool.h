#pragma once
#include "System/Thread/IThreadPool.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <semaphore>
#include <thread>
#include <vector>

namespace System {

class ThreadPool : public IThreadPool
{
public:
    // Constructor
    ThreadPool(int threadCount);
    ~ThreadPool() override;

    // IThreadPool implementation
    void Start() override;
    void Stop() override;
    void Submit(std::function<void()> task) override
    {
        if (_stop.load(std::memory_order_acquire))
            return;

        _tasks.enqueue(std::move(task));
        _taskSemaphore.release();
    }
    size_t GetThreadCount() const override
    {
        return _threads.size();
    }

    // Enqueue a generic callable task. Returns a std::future.
    template <typename Func, typename... Args>
    auto Enqueue(Func &&func, Args &&...args) -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using ResultType = std::invoke_result_t<Func, Args...>;

        // Check stop first
        if (_stop.load(std::memory_order_acquire))
        {
            // Return empty/invalid future or dummy
            return {};
        }

        // Wrap the task in a packaged_task to retrieve the return value asynchronously
        auto task = std::make_shared<std::packaged_task<ResultType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );

        std::future<ResultType> res = task->get_future();

        // Enqueue a lambda that executes the shared packaged_task
        _tasks.enqueue(
            [task]()
            {
                (*task)();
            }
        );

        // Wake up a worker
        _taskSemaphore.release();

        return res;
    }


private:
    int _threadCount;
    std::vector<std::jthread> _threads;
    std::atomic<bool> _stop{false};

    using TaskSemaphore = std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()>;
    TaskSemaphore _taskSemaphore{0};

    // Lock-Free Queue for Tasks
    moodycamel::ConcurrentQueue<std::function<void()>> _tasks;
};

} // namespace System
