#include "System/Session/UDPSession.h"
#include "System/Network/UDPNetworkImpl.h"
#include "System/Network/UDPTransportHeader.h"
#include "System/Network/GenerateUDPToken.h"
#include "System/ILog.h"
#include "System/Pch.h"

namespace System {

struct UDPSessionImpl
{
    boost::asio::ip::udp::endpoint endpoint;
    std::chrono::steady_clock::time_point lastActivity;
    UDPNetworkImpl *network = nullptr;
    uint128_t udpToken = 0;
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
    Session::Reset();

    _id = sessionId;
    _dispatcher = dispatcher;
    _connected.store(true);

    _impl->endpoint = endpoint;
    _impl->lastActivity = std::chrono::steady_clock::now();
    _impl->udpToken = GenerateUDPToken::Generate();

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

void UDPSession::SetNetwork(UDPNetworkImpl *network)
{
    _impl->network = network;
}

UDPNetworkImpl *UDPSession::GetNetwork() const
{
    return _impl->network;
}

void UDPSession::SetUdpToken(uint128_t token)
{
    _impl->udpToken = token;
}

uint128_t UDPSession::GetUdpToken() const
{
    return _impl->udpToken;
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
    if (!_impl->network)
    {
        return;
    }

    PacketMessage *msg;
    while (_sendQueue.try_dequeue(msg))
    {
        const uint16_t packetSize = msg->length;

        std::vector<uint8_t> sendBuffer;
        sendBuffer.resize(UDPTransportHeader::SIZE + packetSize);

        UDPTransportHeader *transportHeader = reinterpret_cast<UDPTransportHeader *>(sendBuffer.data());
        transportHeader->tag = UDPTransportHeader::TAG_RAW_UDP;
        transportHeader->sessionId = _id;
        transportHeader->udpToken = _impl->udpToken;

        std::memcpy(sendBuffer.data() + UDPTransportHeader::SIZE, msg->Payload(), packetSize);

        _impl->network->SendTo(sendBuffer.data(), sendBuffer.size(), _impl->endpoint);

        msg->DecRef();
        if (msg->DecRef())
        {
            if (msg->isPooled)
            {
            }
        }
    }
}

} // namespace System
