#include "System/ILog.h"
#include "System/Monitor/IMonitor.h"
#include "System/Pch.h"


namespace System {

class MonitorImpl : public IMonitor {
public:
  void RecordAccept() override { _acceptCount++; }

  void RecordPacket(uint32_t count) override { _packetPPS += count; }

  void RecordJob() override { _jobCount++; }

  void LogMetrics() override {
    LOG_INFO("[Monitor] Accepts: {}, PPS: {}, Jobs: {}", _acceptCount.load(),
             _packetPPS.load(), _jobCount.load());
    // Reset for next interval if needed, or keep cumulative.
    // For now, let's just log cumulative.
  }

private:
  std::atomic<uint64_t> _acceptCount{0};
  std::atomic<uint64_t> _packetPPS{0};
  std::atomic<uint64_t> _jobCount{0};
};

IMonitor &GetMonitor() {
  static MonitorImpl instance;
  return instance;
}

} // namespace System
