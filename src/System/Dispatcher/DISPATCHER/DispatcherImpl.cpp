#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h"
#include "Share/Protocol.h"
#include "System/ILog.h"
#include "System/Network/ASIO/SessionFactory.h"
#include "System/Pch.h"

namespace System {

// GetDispatcher removed. Dependency Injection used.

DispatcherImpl::DispatcherImpl(std::shared_ptr<IPacketHandler> packetHandler) : _packetHandler(packetHandler) {}

DispatcherImpl::~DispatcherImpl() {}

void DispatcherImpl::Post(SystemMessage message) { _messageQueue.enqueue(message); }

void DispatcherImpl::Process() {
    SystemMessage msg;
    while (_messageQueue.try_dequeue(msg)) {
        switch (msg.type) {
        case MessageType::NETWORK_DATA:
            if (msg.data && msg.data->size() >= sizeof(Share::PacketHeader)) {
                // Zero-Copy & Lock-Free: Use pre-looked-up session
                if (msg.session) {
                    _packetHandler->HandlePacket(msg.session, msg.data);
                } else {
                    // Fallback (Should not happen with current AsioSession logic)
                    // if (auto session = SessionFactory::FindSession(msg.sessionId)) {
                    //     _packetHandler->HandlePacket(session, msg.data);
                    // }
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
}

} // namespace System
