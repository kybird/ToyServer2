#include "System/Network/UDPNetworkImpl.h"
#include "System/Network/UDPEndpointRegistry.h"
#include "System/ISession.h"
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

    if (bytesReceived > 0)
    {
        LOG_INFO("[UDP Network] Received {} bytes from {}:{}", bytesReceived,
                 senderEndpoint.address().to_string(), senderEndpoint.port());

        // Delegate session management to registry
        if (_registry && _dispatcher)
        {
            // Find or create session for this endpoint
            ISession *session = _registry->Find(senderEndpoint);

            if (!session)
            {
                // New session - create and register
                // This is where SessionFactory::CreateUDPSession would be called
                // For now, we'll handle this in Wave 2 when SessionFactory is updated
                LOG_INFO("[UDP Network] New endpoint detected, session creation pending...");
            }
            else
            {
                // Existing session - update activity and handle data
                _registry->UpdateActivity(senderEndpoint);
                // Data would be posted to dispatcher here
            }
        }
    }

    // Continue receiving
    StartReceive();
}

} // namespace System
