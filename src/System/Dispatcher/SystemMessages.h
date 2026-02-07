#pragma once

#include "System/Dispatcher/IMessage.h"
#include "System/ITimer.h"
#include <memory>

namespace System {

// InternalMessageType removed - merged into MessageType

struct ITimerListener;

struct TimerExpiredMessage : public IMessage
{
    TimerExpiredMessage()
    {
        type = MessageType::LOGIC_TIMER_EXPIRED;
    }
    uint64_t timerId;
};

struct TimerTickMessage : public IMessage
{
    TimerTickMessage()
    {
        type = MessageType::LOGIC_TIMER_TICK;
    }
    uint32_t tickCount;
};

struct TimerAddMessage : public IMessage
{
    TimerAddMessage()
    {
        type = MessageType::LOGIC_TIMER_ADD;
    }
    uint64_t timerId;
    uint32_t logicTimerId;
    uint32_t intervalMs;
    bool isInterval;
    ITimerListener *listener;
    std::weak_ptr<ITimerListener> weakListener;
    void *pParam;
};

struct TimerCancelMessage : public IMessage
{
    TimerCancelMessage()
    {
        type = MessageType::LOGIC_TIMER_CANCEL;
    }
    uint64_t timerId;
    ITimerListener *listener;
};

struct ITimerHandler
{
    virtual ~ITimerHandler() = default;
    virtual void OnTimerExpired(uint64_t timerId) = 0;
    virtual void OnTimerAdd(TimerAddMessage *msg) = 0;
    virtual void OnTimerCancel(TimerCancelMessage *msg) = 0;
    virtual void OnTick(TimerTickMessage *msg) = 0;
};

} // namespace System
