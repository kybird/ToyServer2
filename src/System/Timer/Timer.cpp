#include "System/Timer/ITimer.h"
#include <memory>
#include <stdexcept>

namespace System {

static std::shared_ptr<ITimer> g_Timer;

ITimer &GetTimer() {
    if (!g_Timer) {
        throw std::runtime_error("Global Timer not initialized. Call SetGlobalTimer first.");
    }
    return *g_Timer;
}

void SetGlobalTimer(std::shared_ptr<ITimer> timer) { g_Timer = timer; }

} // namespace System
