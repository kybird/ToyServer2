#pragma once

#include <cstdint>

namespace System {

class INetwork {
public:
  virtual ~INetwork() = default;
  virtual bool Start(uint16_t port) = 0;
  virtual void Stop() = 0;
  virtual void Run() = 0; // The Loop
};

} // namespace System
