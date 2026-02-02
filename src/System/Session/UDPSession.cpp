#include "System/Session/UDPSession.h"
#include "System/ILog.h"
#include "System/Pch.h"

namespace System {

struct UDPSessionImpl
{
    boost::asio::ip::udp::endpoint endpoint;
    std::chrono::steady_clock::time_point lastActivity;
};

UDPSession::UDPSession() : _impl(std::make_unique<UDPSessionImpl>())
{
}

UDPSession::~UDPSession()
{
}

void UDPSession::Reset(std::shared_ptr<void> socketVoidPtr, uint64_t sessionId, IDispatcher *dispatcher,
                      const boost::asio::ip::udp::endpoint &endpoint)
{
    // Call base class Reset
    Session::Reset();

    // Initialize base class members
    _id = sessionId;
    _dispatcher = dispatcher;
    _connected.store(true);

    // Store UDP-specific data
    _impl->endpoint = endpoint;
    _impl->lastActivity = std::chrono::steady_clock::now();

    LOG_INFO("[UDPSession] Session {} reset for endpoint {}:{}", _id,
             endpoint.address().to_string(), endpoint.port());
}

const boost::asio::ip::udp::endpoint &UDPSession::GetEndpoint() const
{
    return _impl->endpoint;
}

void UDPSession::UpdateActivity()
{
    _impl->lastActivity = std::chrono::steady_clock::now();
}

const std::chrono::steady_clock::time_point &UDPSession::GetLastActivity() const
{
    return _impl->lastActivity;
}

void UDPSession::HandleData(const uint8_t *data, size_t length)
{
    UpdateActivity();

    // Post packet to dispatcher
    if (_dispatcher)
    {
        // Allocate packet message
        // This follows existing pattern - would need to use MessagePool::Allocate
        // For now, this is placeholder
    }
}

void UDPSession::OnConnect()
{
    LOG_INFO("[UDPSession] Session {} connected from {}:{}", _id,
             _impl->endpoint.address().to_string(), _impl->endpoint.port());

    _connected.store(true);
}

void UDPSession::OnDisconnect()
{
    LOG_INFO("[UDPSession] Session {} disconnected from {}:{}", _id,
             _impl->endpoint.address().to_string(), _impl->endpoint.port());

    _connected.store(false);
}

void UDPSession::Close()
{
    OnDisconnect();
}

void UDPSession::Flush()
{
    // UDP sessions handle sending differently from TCP
    // UDP is connectionless, so "flush" means send queued packets to endpoint
    // This will be implemented when integrating with UDPNetworkImpl

    PacketMessage *msg;
    while (_sendQueue.try_dequeue(msg))
    {
        // Send packet to endpoint
        // This will be called via UDPNetworkImpl::SendTo
        // For now, placeholder

        // Release message
        // msg->Release();
    }
}

} // namespace System
