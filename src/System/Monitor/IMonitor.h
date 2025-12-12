#pragma once

#include <cstdint>

namespace System {

class IMonitor {
public:
  virtual ~IMonitor() = default;
  virtual void RecordAccept() = 0;
  virtual void RecordPacket(uint32_t count = 1) = 0;
  virtual void RecordJob() = 0;
  virtual void LogMetrics() = 0;
};

IMonitor &GetMonitor();

} // namespace System
