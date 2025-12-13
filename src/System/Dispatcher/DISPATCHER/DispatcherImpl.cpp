#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h"
#include "Share/Protocol.h"
#include "System/ILog.h"
#include "System/Network/ASIO/SessionFactory.h"
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
    static const size_t BATCH_SIZE = 64;
    SystemMessage msgs[BATCH_SIZE];

    size_t count = _messageQueue.try_dequeue_bulk(msgs, BATCH_SIZE);

    if (count == 0)
        return false;

    for (size_t i = 0; i < count; ++i)
    {
        SystemMessage &msg = msgs[i];
        switch (msg.type)
        {
        case MessageType::NETWORK_DATA:
            if (msg.data && msg.data->size() >= sizeof(Share::PacketHeader))
            {
                if (msg.session)
                {
                    _packetHandler->HandlePacket(msg.session, msg.data);
                }
                else
                {
                    LOG_ERROR("Dispatcher: NETWORK_DATA without session pointer. ID: {}", msg.sessionId);
                }
            }
            break;
        case MessageType::NETWORK_CONNECT:
            LOG_INFO("Dispatcher: Session {} Connected", msg.sessionId);
            break;
        case MessageType::NETWORK_DISCONNECT:
            LOG_INFO("Dispatcher: Session {} Disconnected", msg.sessionId);
            SessionFactory::RemoveSession(msg.sessionId);
            break;
        default:
            break;
        }
    }

    return true;
}

} // namespace System
