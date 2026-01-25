#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h"
#include "System/Dispatcher/SystemMessages.h"
#include "System/ILog.h"
#include "System/ITimer.h"
#include "System/Packet/PacketHeader.h"
#include "System/PacketView.h" // Added
#include "System/Pch.h"
#include "System/Session/Session.h"
#include "System/Session/SessionFactory.h" // Added for SessionFactory::Destroy
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

    // [Optimization] Smart Notify
    // Only notify if there are threads actually waiting in Wait()
    if (_waitingCount.load(std::memory_order_relaxed) > 0)
    {
        _cv.notify_one();
    }
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
                // session pointer is now ISession* to support Gateway/Backend sessions
                ISession *session = msg->session;
                if (session && session->IsConnected())
                {
                    PacketMessage *content = static_cast<PacketMessage *>(msg);

                    if (content->length >= sizeof(System::PacketHeader))
                    {
                        System::PacketHeader *header = reinterpret_cast<System::PacketHeader *>(content->Payload());

                        // Dispatch packet to handler

                        // [Refactoring] Create PacketView (Body Only)
                        // Strip header so handler only sees the payload
                        PacketView view(
                            header->id,
                            content->Payload() + sizeof(System::PacketHeader),
                            content->length - sizeof(System::PacketHeader)
                        );

                        _packetHandler->HandlePacket(session, view);
                    }
                    else
                    {
                        LOG_ERROR("Packet too small for header: {}", content->length);
                    }
                }
            }
            }
            break;

        case (uint32_t)MessageType::NETWORK_CONNECT:
            // No registry - just notification, session is already managed by Session
            break;

        case (uint32_t)MessageType::NETWORK_DISCONNECT:
            if (msg->session)
            {
                // Notify Logic verify cleanup before destruction
                if (_packetHandler)
                {
                    _packetHandler->OnSessionDisconnect(msg->session);
                }

                // [Invariant] After DISCONNECT, no new NETWORK_DATA messages
                // will be generated for this session. Remaining messages in queue
                // are protected by IncRef and will be safely processed.
                _pendingDestroy.push_back(msg->session);
            }
            break;

        case static_cast<uint32_t>(MessageType::LOGIC_TIMER): {
            // [DEPRECATED]
        }
        break;

        case static_cast<uint32_t>(MessageType::LOGIC_TIMER_EXPIRED): {
            if (_timerHandler)
            {
                TimerExpiredMessage *tMsg = static_cast<TimerExpiredMessage *>(msg);
                _timerHandler->OnTimerExpired(tMsg->timerId);
            }
        }
        break;

        case static_cast<uint32_t>(MessageType::LOGIC_TIMER_ADD): {
            if (_timerHandler)
            {
                TimerAddMessage *tMsg = static_cast<TimerAddMessage *>(msg);
                _timerHandler->OnTimerAdd(tMsg);
            }
        }
        break;

        case static_cast<uint32_t>(MessageType::LOGIC_TIMER_CANCEL): {
            if (_timerHandler)
            {
                TimerCancelMessage *tMsg = static_cast<TimerCancelMessage *>(msg);
                _timerHandler->OnTimerCancel(tMsg);
            }
        }
        break;

        case static_cast<uint32_t>(MessageType::LOGIC_TIMER_TICK): {
            if (_timerHandler)
            {
                TimerTickMessage *tMsg = static_cast<TimerTickMessage *>(msg);
                _timerHandler->OnTick(tMsg);
            }
        }
        break;

        case (uint32_t)MessageType::LAMBDA_JOB: {
            LambdaMessage *lMsg = static_cast<LambdaMessage *>(msg);
            if (lMsg->task)
            {
                lMsg->task();
            }
            // Manual delete since it was allocated with new in Push
            delete lMsg;

            // Prevent MessagePool::Free below which expects Pool-allocated objects
            // OR adapt MessagePool to handle it. Actually, MessagePool::Free handles IMessage*.
            // BUT LambdaMessage is NOT in MessagePool yet.
            // Hack: continue to avoid MessagePool::Free
            continue;
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

void DispatcherImpl::Wait(int timeoutMs)
{
    // [Optimization] Track waiting thread count to avoid unnecessary notify_one calls
    _waitingCount.fetch_add(1, std::memory_order_relaxed);

    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        [this]
        {
            return GetQueueSize() > 0;
        }
    );

    _waitingCount.fetch_sub(1, std::memory_order_relaxed);
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
        ISession *session = _pendingDestroy[i];
        if (session->CanDestroy())
        {
            // Release to pool via factory
            SessionFactory::Destroy(session);

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

void DispatcherImpl::Push(std::function<void()> task)
{
    // [Optimization Decision]
    // 벤치마크 결과(Legacy: 48ms vs 4KB Pooling: 181ms), 소형 객체인 LambdaMessage를
    // 거대한 4KB 풀에 넣는 것은 심각한 캐시 미스와 내부 단편화를 유발하여 성능이 오히려 하락함.
    // 따라서 Windows LFH(Low Fragmentation Heap)에 최적화된 시스템 할당자를 사용하도록 롤백함.
    LambdaMessage *msg = new LambdaMessage();
    msg->task = std::move(task);
    Post(msg);
}

} // namespace System
