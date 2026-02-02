#pragma once

#include "System/Pch.h"
#include <boost/asio/ip/udp.hpp>
#include <memory>

namespace System {

class IDispatcher;
class UDPEndpointRegistry;

/**
 * @brief UDP network implementation class.
 * Manages UDP socket, receives packets, and delegates session management to UDPEndpointRegistry.
 */
class UDPNetworkImpl
{
public:
    UDPNetworkImpl(boost::asio::io_context &ioContext);
    ~UDPNetworkImpl();

    /**
     * @brief Start UDP listener on specified port.
     * @param port UDP port to bind to
     * @return true if started successfully, false otherwise
     */
    bool Start(uint16_t port);

    /**
     * @brief Stop UDP listener.
     */
    void Stop();

    /**
     * @brief Set the session registry for endpoint-to-session mapping.
     * @param registry Pointer to UDPEndpointRegistry
     */
    void SetRegistry(UDPEndpointRegistry *registry);

    /**
     * @brief Set the dispatcher for posting received packets.
     * @param dispatcher Pointer to IDispatcher
     */
    void SetDispatcher(IDispatcher *dispatcher);

    /**
     * @brief Send UDP data to specific endpoint.
     * @param data Data to send
     * @param length Length of data
     * @param destination Target endpoint
     * @return true if send initiated successfully
     */
    bool SendTo(const uint8_t *data, size_t length, const boost::asio::ip::udp::endpoint &destination);

private:
    /**
     * @brief Start asynchronous receive from UDP socket.
     */
    void StartReceive();

    /**
     * @brief Handle received UDP data.
     * @param error Error code
     * @param bytesReceived Number of bytes received
     * @param senderEndpoint Endpoint of the sender
     */
    void HandleReceive(const boost::system::error_code &error, size_t bytesReceived, const boost::asio::ip::udp::endpoint &senderEndpoint);

private:
    boost::asio::io_context &_ioContext;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _senderEndpoint;
    UDPEndpointRegistry *_registry = nullptr;
    IDispatcher *_dispatcher = nullptr;
    std::atomic<bool> _isStopping{false};

    // Receive buffer (larger than typical MTU for safety)
    std::array<uint8_t, 65536> _receiveBuffer;
};

} // namespace System
