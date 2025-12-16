#pragma once

#include "System/Dispatcher/IMessage.h"
#include "System/ITimer.h"
#include <memory>

namespace System {

enum class InternalMessageType {
    IMT_LOGIC_TIMER = 1000,
    IMT_LOGIC_TIMER_EXPIRED,
    IMT_LOGIC_TIMER_ADD,
    IMT_LOGIC_TIMER_CANCEL,
    IMT_LOGIC_TIMER_TICK
};

struct ITimerListener;

struct TimerMessage : public IMessage
{
    TimerMessage()
    {
        type = (uint32_t)InternalMessageType::IMT_LOGIC_TIMER;
    }
    uint32_t timerId;
    void *pParam;
    ITimerListener *listener;
    std::shared_ptr<void> lifetimeRef;
};

struct TimerExpiredMessage : public IMessage
{
    TimerExpiredMessage()
    {
        type = (uint32_t)InternalMessageType::IMT_LOGIC_TIMER_EXPIRED;
    }
    uint64_t timerId;
};

struct TimerTickMessage : public IMessage
{
    TimerTickMessage()
    {
        type = (uint32_t)InternalMessageType::IMT_LOGIC_TIMER_TICK;
    }
    uint32_t tickCount;
};

struct TimerAddMessage : public IMessage
{
    TimerAddMessage()
    {
        type = (uint32_t)InternalMessageType::IMT_LOGIC_TIMER_ADD;
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
        type = (uint32_t)InternalMessageType::IMT_LOGIC_TIMER_CANCEL;
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
