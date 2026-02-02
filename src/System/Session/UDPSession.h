#pragma once

#include "System/Session/Session.h"
#include "System/Pch.h"
#include <boost/asio/ip/udp.hpp>
#include <memory>

namespace System {

class IDispatcher;

struct UDPSessionImpl;

/**
 * @brief UDP session implementation extending Session base class.
 * Manages UDP-specific session behavior including endpoint tracking and timeout handling.
 */
class UDPSession : public Session
{
    friend struct UDPSessionImpl;

public:
    UDPSession();
    virtual ~UDPSession();

    /**
     * @brief Reset session with UDP-specific parameters.
     * @param socketVoidPtr Void pointer to UDP socket
     * @param sessionId Session ID
     * @param dispatcher Dispatcher reference
     * @param endpoint Remote UDP endpoint
     */
    void Reset(std::shared_ptr<void> socketVoidPtr, uint64_t sessionId, IDispatcher *dispatcher,
              const boost::asio::ip::udp::endpoint &endpoint);

    /**
     * @brief Get the remote UDP endpoint.
     * @return Remote endpoint
     */
    const boost::asio::ip::udp::endpoint &GetEndpoint() const;

    /**
     * @brief Update last activity time.
     */
    void UpdateActivity();

    /**
     * @brief Get last activity time.
     * @return Timestamp of last activity
     */
    const std::chrono::steady_clock::time_point &GetLastActivity() const;

    /**
     * @brief Handle incoming data.
     * @param data Data received
     * @param length Data length
     */
    void HandleData(const uint8_t *data, size_t length);

    // ISession overrides
    void OnConnect() override;
    void OnDisconnect() override;
    void Close() override;

protected:
    void Flush() override;

private:
    std::unique_ptr<UDPSessionImpl> _impl;
};

} // namespace System
