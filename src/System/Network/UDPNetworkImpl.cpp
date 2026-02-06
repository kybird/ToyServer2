#include "System/Network/UDPNetworkImpl.h"
#include "System/Network/UDPEndpointRegistry.h"
#include "System/ISession.h"
#include "System/Session/UDPSession.h"
#include "System/ILog.h"
#include "System/Pch.h"

namespace System {

UDPNetworkImpl::UDPNetworkImpl(boost::asio::io_context &ioContext)
    : _ioContext(ioContext), _socket(_ioContext)
{
}

UDPNetworkImpl::~UDPNetworkImpl()
{
    Stop();
}

bool UDPNetworkImpl::Start(uint16_t port)
{
    _isStopping.store(false);
    try
    {
        // Bind explicitly to 127.0.0.1 to avoid ambiguities
        boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), port);

        _socket.open(endpoint.protocol());
        // Disable reuse_address to detect zombie processes
        _socket.set_option(boost::asio::socket_base::reuse_address(false));
        _socket.bind(endpoint);

        LOG_INFO("UDP Network listening on 127.0.0.1:{}", port);

        StartReceive();

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[DEBUG] UDP Network Start Failed on Port " << port << ": " << e.what() << std::endl;
        LOG_ERROR("UDP Network Start Failed: {}", e.what());
        return false;
    }
}

void UDPNetworkImpl::Stop()
{
    _isStopping.store(true);
    boost::system::error_code ec;
    if (_socket.is_open())
    {
        _socket.close(ec);
    }
}

void UDPNetworkImpl::SetRegistry(UDPEndpointRegistry *registry)
{
    _registry = registry;
}

void UDPNetworkImpl::SetDispatcher(IDispatcher *dispatcher)
{
    _dispatcher = dispatcher;
}

bool UDPNetworkImpl::SendTo(const uint8_t *data, size_t length, const boost::asio::ip::udp::endpoint &destination)
{
    if (!_socket.is_open() || _isStopping.load())
    {
        return false;
    }

    try
    {
        _socket.async_send_to(
            boost::asio::buffer(data, length),
            destination,
            [](const boost::system::error_code &error, size_t /*bytesSent*/)
            {
                if (error)
                {
                    LOG_ERROR("UDP Send Error: {}", error.message());
                }
            }
        );
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("UDP Send Exception: {}", e.what());
        return false;
    }
}

void UDPNetworkImpl::StartReceive()
{
    if (!_socket.is_open())
        return;

    _socket.async_receive_from(
        boost::asio::buffer(_receiveBuffer),
        _senderEndpoint,
        [this](const boost::system::error_code &error, size_t bytesReceived)
        {
            HandleReceive(error, bytesReceived, _senderEndpoint);
        }
    );
}

void UDPNetworkImpl::HandleReceive(const boost::system::error_code &error, size_t bytesReceived,
                                const boost::asio::ip::udp::endpoint &senderEndpoint)
{
    if (_isStopping.load())
    {
        LOG_INFO("[UDP Network] Receive handler triggered but _isStopping is TRUE. Discarding packet.");
        return;
    }

    if (error)
    {
        if (error == boost::asio::error::operation_aborted)
        {
            LOG_INFO("[UDP Network] Receive operation aborted (expected during stop).");
        }
        else
        {
            LOG_ERROR("[UDP Network] Receive Error: {}. Re-trying...", error.message());
        }
        return;
    }

    if (bytesReceived > UDPTransportHeader::SIZE)
    {
        LOG_INFO("[UDP Network] Received {} bytes from {}:{}", bytesReceived,
                 senderEndpoint.address().to_string(), senderEndpoint.port());

        if (_registry && _dispatcher)
        {
            const UDPTransportHeader *transportHeader = reinterpret_cast<const UDPTransportHeader *>(_receiveBuffer.data());

            if (!transportHeader->IsValid())
            {
                LOG_WARN("[UDP Network] Invalid transport header tag: {}, discarding packet", transportHeader->tag);
                StartReceive();
                return;
            }

            LOG_INFO("[UDP Network] Transport Header - SessionId: {}, Tag: {}", transportHeader->sessionId, transportHeader->tag);

            ISession *session = nullptr;

            session = _registry->Find(senderEndpoint);

            if (!session)
            {
                session = _registry->GetEndpointByToken(transportHeader->udpToken);

                if (session)
                {
                    LOG_INFO("[UDP Network] NAT rebinding detected - session {} moved to new endpoint {}:{}", transportHeader->sessionId,
                             senderEndpoint.address().to_string(), senderEndpoint.port());
                }
            }

            if (!session)
            {
                LOG_INFO("[UDP Network] New endpoint or invalid token, session creation pending...");
                StartReceive();
                return;
            }

            _registry->UpdateActivity(senderEndpoint);

            const uint8_t *packetData = _receiveBuffer.data() + UDPTransportHeader::SIZE;
            size_t packetLength = bytesReceived - UDPTransportHeader::SIZE;

            UDPSession *udpSession = dynamic_cast<UDPSession *>(session);
            if (udpSession)
            {
                udpSession->HandleData(packetData, packetLength);
            }
        }
    }
    else if (bytesReceived > 0)
    {
        LOG_WARN("[UDP Network] Packet too small ({} bytes), expected at least {} bytes", bytesReceived, UDPTransportHeader::SIZE);
    }

    StartReceive();
}

} // namespace System
