#pragma once
#include "System/Memory/ObjectPool.h"
#include "System/Timer/ITimer.h"
#include "System/Timer/TimerHandle.h"
#include <boost/asio.hpp>
#include <memory>


namespace System {

class AsioTimer : public ITimer, public std::enable_shared_from_this<AsioTimer> {
public:
    AsioTimer(boost::asio::io_context &ioContext);
    virtual ~AsioTimer();

    void SetTimer(std::chrono::milliseconds delay, std::function<void()> callback) override;
    void SetInterval(std::chrono::milliseconds interval, std::function<void()> callback) override;

private:
    std::shared_ptr<TimerHandle> SetTimeout(std::chrono::milliseconds delay, std::function<void()> callback);

private:
    boost::asio::io_context &_ioContext;
    ObjectPool<TimerHandle> _handlePool;
};

} // namespace System
