#include "System/Timer/ASIO/AsioTimer.h"
#include "System/Pch.h"

namespace System {

AsioTimer::AsioTimer(boost::asio::io_context &ioContext) : _ioContext(ioContext)
{
}

AsioTimer::~AsioTimer()
{
    // No explicit cleanup needed for pool, shared_ptrs will naturally die
    // when all references are gone.
    // However, pending async operations might still hold references.
    // This is fine as io_context shutdown will handle them or they will just complete/cancel.
}

void AsioTimer::SetTimer(std::chrono::milliseconds delay, std::function<void()> callback)
{
    SetTimeout(delay, callback);
}

void AsioTimer::SetInterval(std::chrono::milliseconds interval, std::function<void()> callback)
{
    // For interval, we recursively call SetTimeout.
    // Optimization: We could reuse the SAME handle, but SetTimeout returns a shared_ptr
    // that returns to pool on destruction.
    // To support "Interval", we can just pass a recursive lambda.

    // We need to keep the recursion alive.
    // The issue with recursion and smart pointers is cyclic dependency if not careful.
    // But here, 'SetTimeout' schedules a NEW async wait.

    // Recursive lambda wrapper
    auto recursive = std::make_shared<std::function<void()>>();
    *recursive = [this, interval, callback, recursive]()
    {
        callback();
        // Reschedule
        SetTimeout(interval, *recursive);
    };

    SetTimeout(interval, *recursive);
}

std::shared_ptr<TimerHandle> AsioTimer::SetTimeout(std::chrono::milliseconds delay, std::function<void()> callback)
{
    // 1. Pop from Pool (Zero Allocation)
    TimerHandle *rawHandle = _handlePool.Pop();

    // 2. Reuse or Create steady_timer
    if (!rawHandle->_timer)
    {
        rawHandle->_timer = std::make_shared<boost::asio::steady_timer>(_ioContext);
    }
    rawHandle->_timer->expires_after(delay);

    // 3. Create shared_ptr with Custom Deleter (Return to pool)
    // IMPORTANT: The deleter must NOT capture 'this' strongly if AsioTimer can move (it can't, it's shared_from_this)
    // But 'this' is guaranteed to outlive the pool usage?
    // If AsioTimer dies, _handlePool dies. usage of 'this' in deleter is dangerous if AsioTimer is already destroyed.
    // ACTUALLY: If AsioTimer is destroyed, we can't push back to its pool.
    // So we should capture a weak_ptr or use shared_from_this?
    // Or, simplistically, assuming AsioTimer outlives all timers (Global Timer).
    // For safety, let's capture 'this' checks? No, `_handlePool` is member.
    // If AsioTimer is destroyed, pending handlers might try to return to pool.
    // To fix this: We can use a trick or just accept that Global Timer lives forever.
    // For now, simple approach:
    std::shared_ptr<AsioTimer> self = shared_from_this();

    std::shared_ptr<TimerHandle> handle(
        rawHandle,
        [self](TimerHandle *ptr)
        {
            ptr->Cancel();               // Ensure cleanup
            self->_handlePool.Push(ptr); // Back to pool
        }
    );

    // 4. Async Wait with Recycling Allocator (Zero Allocation for Handler)
    auto timer = rawHandle->_timer;

    // We need to keep 'handle' alive until callback runs.
    // Capture 'handle' in lambda.
    timer->async_wait(
        boost::asio::bind_allocator(
            boost::asio::recycling_allocator<void>(),
            [handle, callback](const boost::system::error_code &ec)
            {
                if (!ec)
                    callback();
                // 'handle' goes out of scope here, deleter runs, returns to pool.
            }
        )
    );

    return handle;
}

} // namespace System
