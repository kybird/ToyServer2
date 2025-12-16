#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h"
#include "Share/Protocol.h"
#include "System/Dispatcher/SystemMessages.h"
#include "System/ILog.h"
#include "System/ITimer.h"
#include "System/Pch.h"
#include "System/Session/Session.h"
#include "System/Session/SessionPool.h"


#include "System/Debug/MemoryMetrics.h"
#include "System/Dispatcher/MessagePool.h"

namespace System {

DispatcherImpl::DispatcherImpl(std::shared_ptr<IPacketHandler> packetHandler) : _packetHandler(packetHandler)
{
}

DispatcherImpl::~DispatcherImpl()
{
}

void DispatcherImpl::Post(IMessage *message)
{
    _messageQueue.enqueue(message);
}

bool DispatcherImpl::Process()
{
    // [Phase 1] Process Messages
    static const size_t BATCH_SIZE = 64;
    IMessage *msgs[BATCH_SIZE];

    size_t count = _messageQueue.try_dequeue_bulk(msgs, BATCH_SIZE);

    if (count > 0)
    {
        for (size_t i = 0; i < count; ++i)
        {
            IMessage *msg = msgs[i];

#ifdef ENABLE_DIAGNOSTICS
            System::Debug::MemoryMetrics::Processed.fetch_add(1, std::memory_order_relaxed);
#endif

            switch (msg->type)
            {
            case (uint32_t)MessageType::NETWORK_DATA: {
                // [Hot Path] Direct Session access - no map lookup, no vtable
                // session pointer is concrete Session*, not ISession*
                Session *session = msg->session;
                if (session && session->IsConnected())
                {
                    PacketMessage *content = static_cast<PacketMessage *>(msg);
                    _packetHandler->HandlePacket(session, content);
                }
            }
            break;

            case (uint32_t)MessageType::NETWORK_CONNECT:
                // No registry - just notification, session is already managed by Session
                break;

            case (uint32_t)MessageType::NETWORK_DISCONNECT:
                if (msg->session)
                {
                    // [Invariant] After DISCONNECT, no new NETWORK_DATA messages
                    // will be generated for this session. Remaining messages in queue
                    // are protected by IncRef and will be safely processed.
                    _pendingDestroy.push_back(msg->session);
                }
                break;

            case (uint32_t)InternalMessageType::IMT_LOGIC_TIMER: {
                // [DEPRECATED]
            }
            break;

            case (uint32_t)InternalMessageType::IMT_LOGIC_TIMER_EXPIRED: {
                if (_timerHandler)
                {
                    TimerExpiredMessage *tMsg = static_cast<TimerExpiredMessage *>(msg);
                    _timerHandler->OnTimerExpired(tMsg->timerId);
                }
            }
            break;

            case (uint32_t)InternalMessageType::IMT_LOGIC_TIMER_ADD: {
                if (_timerHandler)
                {
                    TimerAddMessage *tMsg = static_cast<TimerAddMessage *>(msg);
                    _timerHandler->OnTimerAdd(tMsg);
                }
            }
            break;

            case (uint32_t)InternalMessageType::IMT_LOGIC_TIMER_CANCEL: {
                if (_timerHandler)
                {
                    TimerCancelMessage *tMsg = static_cast<TimerCancelMessage *>(msg);
                    _timerHandler->OnTimerCancel(tMsg);
                }
            }
            break;

            case (uint32_t)InternalMessageType::IMT_LOGIC_TIMER_TICK: {
                if (_timerHandler)
                {
                    TimerTickMessage *tMsg = static_cast<TimerTickMessage *>(msg);
                    _timerHandler->OnTick(tMsg);
                }
            }
            break;

            default:
                break;
            }

            // [Lifetime] Release session reference (matches IncRef before Post)
            if (msg->session)
            {
                msg->session->DecRef();
            }

            // Return to pool
            MessagePool::Free(msg);
        }
    }

    // [Phase 2] Process Lifecycle (Deferred Destruction)
    ProcessPendingDestroys();

    return count > 0;
}

void DispatcherImpl::ProcessPendingDestroys()
{
    if (_pendingDestroy.empty())
        return;

    // =========================================================================
    // [OPTIMIZATION] Swap-and-Pop Deletion - O(1) per element
    // =========================================================================
    // Standard vector::erase is O(n) because it shifts all following elements.
    // Swap-and-pop achieves O(1) by:
    //   1. Swapping the target with the last element
    //   2. Popping the last element (O(1))
    // Order is not preserved, but _pendingDestroy order doesn't matter.
    //
    // DO NOT REFACTOR TO erase() - This is intentional optimization.
    // =========================================================================

    size_t i = 0;
    while (i < _pendingDestroy.size())
    {
        Session *session = _pendingDestroy[i];
        if (session->CanDestroy())
        {
            // Release to pool
            SessionPool<Session>::Release(session);

            // Swap-and-pop: O(1) deletion
            _pendingDestroy[i] = _pendingDestroy.back();
            _pendingDestroy.pop_back();
            // Don't increment i - check the swapped element at same position
        }
        else
        {
            ++i;
        }
    }
}

// DispatcherImpl.h Implementations
size_t DispatcherImpl::GetQueueSize() const
{
    return _messageQueue.size_approx();
}

bool DispatcherImpl::IsOverloaded() const
{
    return _messageQueue.size_approx() > HIGH_WATER;
}

bool DispatcherImpl::IsRecovered() const
{
    return _messageQueue.size_approx() < LOW_WATER;
}

void DispatcherImpl::RegisterTimerHandler(ITimerHandler *handler)
{
    _timerHandler = handler;
}

} // namespace System
