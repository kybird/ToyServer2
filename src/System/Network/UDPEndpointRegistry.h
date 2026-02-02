#pragma once

#include "System/Pch.h"
#include <boost/asio/ip/udp.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace System {

class ISession;

/**
 * @brief Registry for mapping UDP endpoints to sessions.
 * Thread-safe container for managing UDP session lifecycle.
 */
class UDPEndpointRegistry
{
public:
    UDPEndpointRegistry();
    ~UDPEndpointRegistry();

    /**
     * @brief Register a session for a specific endpoint.
     * @param endpoint UDP endpoint
     * @param session Session to register
     */
    void Register(const boost::asio::ip::udp::endpoint &endpoint, ISession *session);

    /**
     * @brief Find session by endpoint.
     * @param endpoint UDP endpoint to search
     * @return Session pointer if found, nullptr otherwise
     */
    ISession *Find(const boost::asio::ip::udp::endpoint &endpoint);

    /**
     * @brief Remove session from registry.
     * @param endpoint UDP endpoint to remove
     */
    void Remove(const boost::asio::ip::udp::endpoint &endpoint);

    /**
     * @brief Update last activity time for a session.
     * @param endpoint UDP endpoint
     */
    void UpdateActivity(const boost::asio::ip::udp::endpoint &endpoint);

    /**
     * @brief Remove sessions that have timed out.
     * @param timeoutMs Timeout threshold in milliseconds
     * @return Number of sessions removed
     */
    size_t CleanupTimeouts(uint32_t timeoutMs);

private:
    // Endpoint-to-session mapping
    struct SessionInfo
    {
        ISession *session = nullptr;
        std::chrono::steady_clock::time_point lastActivity;
    };

    std::unordered_map<boost::asio::ip::udp::endpoint, SessionInfo> _sessions;
    std::mutex _mutex;
};

} // namespace System
