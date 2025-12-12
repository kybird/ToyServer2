#include "System/Thread/ThreadPool.h"
#include "System/ILog.h"
#include "System/Pch.h"


namespace System {

ThreadPool::ThreadPool(int threadCount) : _threadCount(threadCount) {}

ThreadPool::~ThreadPool() { Stop(); }

void ThreadPool::Start(std::function<void()> workerTask) {
  LOG_INFO("ThreadPool Starting with {} threads...", _threadCount);
  _threads.reserve(_threadCount);
  for (int i = 0; i < _threadCount; ++i) {
    _threads.emplace_back([i, workerTask, this]() {
      LOG_INFO("Worker Thread #{} Started", i);
      try {
        workerTask();
      } catch (const std::exception &e) {
        LOG_ERROR("Worker Thread #{} Exception: {}", i, e.what());
      }
      LOG_INFO("Worker Thread #{} Stopped", i);
    });
  }
}

void ThreadPool::Stop() {
  if (_stopped)
    return;
  _stopped = true;
  // jthread destructor handles join automatically, but we can call explicit
  // join if needed logic suggests Usually we need to Signal the io_context to
  // stop FIRST before joining. That logic will be in the caller (AsioService),
  // or we can pass io_context here but strict separation suggests ThreadPool
  // just runs a task.
}

void ThreadPool::Join() {
  // jthread joins on destruction, or we can explicit join loop
  for (auto &t : _threads) {
    if (t.joinable())
      t.join();
  }
  _threads.clear();
}

} // namespace System
