#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <vector>


namespace System {

class ThreadPool {
public:
  ThreadPool(int threadCount);
  ~ThreadPool();

  void Start(std::function<void()> workerTask);
  void Stop();
  void Join();

  // Access to underlying threads if needed (rarely)
  size_t GetThreadCount() const { return _threads.size(); }

private:
  int _threadCount;
  std::vector<std::jthread> _threads;
  bool _stopped = false;
};

} // namespace System
