#pragma once

#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace System {

class ThreadPool
{
public:
    // Constructor
    ThreadPool(int threadCount);
    ~ThreadPool();

    // Initialize and start workers
    void Start();

    // Stop the thread pool.
    // wait = true: Process all remaining tasks before exiting (FINISH_PENDING).
    // wait = false: Exit as soon as possible (ABORT_PENDING).
    void Stop(bool wait = true);

    // Enqueue a generic callable task. Returns a std::future.
    template <typename Func, typename... Args>
    auto Enqueue(Func &&func, Args &&...args) -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using ResultType = std::invoke_result_t<Func, Args...>;

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

        return res;
    }

    // Access specific thread count info
    size_t GetThreadCount() const
    {
        return _threads.size();
    }

private:
    int _threadCount;
    std::vector<std::jthread> _threads;
    std::atomic<bool> _running = false;
    std::atomic<bool> _stop = false;

    // Lock-Free Queue for Tasks
    moodycamel::ConcurrentQueue<std::function<void()>> _tasks;
};

} // namespace System
