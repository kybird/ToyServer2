#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Dispatcher/SystemMessages.h"
#include "System/ILog.h"
#include "System/Packet/PacketHeader.h"
#include "System/PacketView.h"
#include "System/Session/SessionContext.h"
#include "System/Session/SessionFactory.h"

namespace System {

DispatcherImpl::DispatcherImpl(std::shared_ptr<IPacketHandler> packetHandler) : _packetHandler(std::move(packetHandler))
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
            case MessageType::LOGIC_JOB:
                // Ignored for now or handled elsewhere
                break;

            case MessageType::NETWORK_DATA:
            case MessageType::PACKET:
                HandlePacketMessage(msg);
                break;

            case MessageType::NETWORK_CONNECT:
                if (msg->session != nullptr)
                {
                    _sessions[msg->sessionId] = msg->session;
                }
                break;

            case MessageType::NETWORK_DISCONNECT:
                if (msg->session != nullptr)
                {
                    _sessions.erase(msg->sessionId);
                    if (_packetHandler != nullptr)
                    {
                        SessionContext ctx(msg->session);
                        _packetHandler->OnSessionDisconnect(std::move(ctx));
                    }
                    _pendingDestroy.push_back(msg->session);
                }
                break;

            case MessageType::LOGIC_TIMER:
                HandleTimerMessage(msg);
                break;

            case MessageType::LOGIC_TIMER_EXPIRED:
                HandleTimerExpiredMessage(msg);
                break;

            case MessageType::LOGIC_TIMER_ADD:
                HandleTimerAddMessage(msg);
                break;

            case MessageType::LOGIC_TIMER_CANCEL:
                HandleTimerCancelMessage(msg);
                break;

            case MessageType::LOGIC_TIMER_TICK:
                HandleTimerTickMessage(msg);
                break;

            case MessageType::LAMBDA_JOB:
                HandleLambdaMessage(msg);
                continue; // Skip MessagePool::Free and DecRef (handled in HandleLambdaMessage)

            default:
                LOG_INFO("Unhandled message type: {}", static_cast<uint32_t>(msg->type));
                break;
            }

            // [Lifetime] Release session reference (matches IncRef before Post)
            if (msg->session != nullptr)
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

void DispatcherImpl::HandlePacketMessage(IMessage *msg)
{
    ISession *session = msg->session;
    if (session != nullptr && session->IsConnected())
    {
        PacketMessage *content = static_cast<PacketMessage *>(msg);
        if (content->length >= sizeof(System::PacketHeader))
        {
            auto *header = reinterpret_cast<System::PacketHeader *>(content->Payload());
            PacketView view(
                header->id,
                content->Payload() + sizeof(System::PacketHeader),
                content->length - sizeof(System::PacketHeader)
            );

            if (_packetHandler != nullptr)
            {
                // [SessionContext Refactoring] Create context and pass by value (move)
                SessionContext ctx(session);
                _packetHandler->HandlePacket(std::move(ctx), view);
            }
        }
        else
        {
            LOG_ERROR("Packet too small for header: {}", content->length);
        }
    }
}

void DispatcherImpl::HandleTimerMessage(IMessage * /*msg*/)
{
    // [DEPRECATED]
}

void DispatcherImpl::HandleTimerExpiredMessage(IMessage *msg)
{
    if (_timerHandler != nullptr)
    {
        auto *tMsg = static_cast<TimerExpiredMessage *>(msg);
        _timerHandler->OnTimerExpired(tMsg->timerId);
    }
}

void DispatcherImpl::HandleTimerAddMessage(IMessage *msg)
{
    if (_timerHandler != nullptr)
    {
        auto *tMsg = static_cast<TimerAddMessage *>(msg);
        _timerHandler->OnTimerAdd(tMsg);
    }
}

void DispatcherImpl::HandleTimerCancelMessage(IMessage *msg)
{
    if (_timerHandler != nullptr)
    {
        auto *tMsg = static_cast<TimerCancelMessage *>(msg);
        _timerHandler->OnTimerCancel(tMsg);
    }
}

void DispatcherImpl::HandleTimerTickMessage(IMessage *msg)
{
    if (_timerHandler != nullptr)
    {
        auto *tMsg = static_cast<TimerTickMessage *>(msg);
        _timerHandler->OnTick(tMsg);
    }
}

void DispatcherImpl::HandleLambdaMessage(IMessage *msg)
{
    auto *lMsg = static_cast<LambdaMessage *>(msg);
    if (lMsg->task)
    {
        lMsg->task();
    }

    if (msg->session != nullptr)
    {
        msg->session->DecRef();
    }

    // LambdaMessage is not pooled, allocated with 'new' in Push()
    delete lMsg;
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
    return _messageQueue.size_approx() > DispatcherImpl::HIGH_WATER;
}

bool DispatcherImpl::IsRecovered() const
{
    return _messageQueue.size_approx() < DispatcherImpl::LOW_WATER;
}

void DispatcherImpl::RegisterTimerHandler(ITimerHandler *handler)
{
    _timerHandler = handler;
}

void DispatcherImpl::WithSession(uint64_t sessionId, std::function<void(SessionContext &)> callback)
{
    // [SessionContext Refactoring] Use Push to ensure callback runs in Dispatcher's logic context (serial)
    Push(
        [this, sessionId, callback = std::move(callback)]()
        {
            auto it = _sessions.find(sessionId);
            if (it != _sessions.end() && it->second->IsConnected())
            {
                SessionContext ctx(it->second);
                callback(ctx);
            }
        }
    );
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

void DispatcherImpl::Shutdown()
{
    // Wake up any threads waiting in Wait()
    _cv.notify_all();
}

} // namespace System
