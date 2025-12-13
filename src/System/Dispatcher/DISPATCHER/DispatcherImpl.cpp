#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h"
#include "Share/Protocol.h"
#include "System/ILog.h"
#include "System/Network/ASIO/AsioSession.h" // Needed for SessionPool<AsioSession> cast
#include "System/Network/ASIO/SessionPool.h"
#include "System/Network/ISession.h"
#include "System/Pch.h"


namespace System {

DispatcherImpl::DispatcherImpl(std::shared_ptr<IPacketHandler> packetHandler) : _packetHandler(packetHandler)
{
}

DispatcherImpl::~DispatcherImpl()
{
}

void DispatcherImpl::Post(SystemMessage message)
{
    _messageQueue.enqueue(message);
}

bool DispatcherImpl::Process()
{
    // [Phase 1] Process Messages
    static const size_t BATCH_SIZE = 64;
    SystemMessage msgs[BATCH_SIZE];

    size_t count = _messageQueue.try_dequeue_bulk(msgs, BATCH_SIZE);

    if (count > 0)
    {
        for (size_t i = 0; i < count; ++i)
        {
            SystemMessage &msg = msgs[i];

            switch (msg.type)
            {
            case MessageType::NETWORK_DATA:
                if (auto it = _sessionRegistry.find(msg.sessionId); it != _sessionRegistry.end())
                {
                    if (msg.data && msg.data->size > sizeof(Share::PacketHeader))
                    {
                        _packetHandler->HandlePacket(it->second, msg.data);
                    }
                }
                break;

            case MessageType::NETWORK_CONNECT:
                if (msg.session)
                {
                    ISession *session = msg.session;
                    _sessionRegistry[msg.sessionId] = session;
                    LOG_INFO(
                        "Dispatcher: Session {} Connected. Registry Size: {}", msg.sessionId, _sessionRegistry.size()
                    );
                }
                break;

            case MessageType::NETWORK_DISCONNECT:
                if (msg.session)
                {
                    ISession *session = msg.session;
                    _sessionRegistry.erase(msg.sessionId);

                    // Defer destruction: pool로 반환
                    _pendingDestroy.push_back(session);

                    LOG_INFO(
                        "Dispatcher: Session {} Disconnected. Pending Destroy: {}",
                        msg.sessionId,
                        _pendingDestroy.size()
                    );
                }
                break;

            default:
                break;
            }
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

    auto it = _pendingDestroy.begin();
    while (it != _pendingDestroy.end())
    {
        ISession *session = *it;
        if (session->CanDestroy())
        {
            // Returning to pool requires casting to concrete type
            SessionPool<AsioSession>::Release(static_cast<AsioSession *>(session));
            it = _pendingDestroy.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace System
