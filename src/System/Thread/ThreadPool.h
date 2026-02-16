#pragma once

#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

namespace System {

class ThreadPool
{
public:
    // Constructor
    ThreadPool(int threadCount, const std::string &name = "ThreadPool (Task)");
    ~ThreadPool();

    // Initialize and start workers
    void Start();

    // Stop the thread pool.
    // Signals threads to exit as soon as they finish current task.
    // New tasks will be rejected.
    void Stop();

    // Join threads and wait for completion.
    void Join();

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

    // Access specific thread count info
    size_t GetThreadCount() const
    {
        return _threads.size();
    }

private:
    int _threadCount;
    std::string _name;
    std::vector<std::jthread> _threads;
    std::atomic<bool> _stop{false};

    using TaskSemaphore = std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()>;
    TaskSemaphore _taskSemaphore{0};

    // Lock-Free Queue for Tasks
    moodycamel::ConcurrentQueue<std::function<void()>> _tasks;
};

} // namespace System
