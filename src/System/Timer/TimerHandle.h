#pragma once
#include <boost/asio.hpp>
#include <memory>


namespace System {

struct TimerHandle {
    // Reuse this timer object repeatedly
    std::shared_ptr<boost::asio::steady_timer> _timer;

    void Cancel() {
        if (_timer)
            _timer->cancel();
    }
};

} // namespace System
