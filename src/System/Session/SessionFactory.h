#pragma once

#ifndef TOY_SERVER_ISESSION_H
#include "System/ISession.h"
#endif
#include "System/Network/IPacketEncryption.h"
#include "System/Pch.h"
#include <atomic>
#include <functional>

namespace System {

// Server role determines which session type to create
enum class ServerRole {
    Gateway, // External clients, encryption required
    Backend  // Internal servers, plain-text, 0-copy optimized
};

class IDispatcher;

class SessionFactory
{
public:
    // [Updated] Returns ISession* to support both Gateway and Backend sessions
    static ISession *CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, IDispatcher *dispatcher);

    // [Restored] Required by DispatcherImpl
    static void Destroy(ISession *session);

    // [Encryption]
    static void SetEncryptionFactory(std::function<std::unique_ptr<IPacketEncryption>()> factory);

    // [RateLimiter Config]
    static void SetRateLimitConfig(double rate, double burst);
    static double GetRateLimit()
    {
        return _rateLimit;
    }
    static double GetRateBurst()
    {
        return _rateBurst;
    }

    // [Heartbeat Config]
    static void SetHeartbeatConfig(uint32_t intervalMs, uint32_t timeoutMs, std::function<void(ISession *)> pingFunc);

    // [Server Role Config]
    static void SetServerRole(ServerRole role);
    static ServerRole GetServerRole()
    {
        return _serverRole;
    }

private:
    static std::atomic<uint64_t> _nextSessionId;
    static std::function<std::unique_ptr<IPacketEncryption>()> _encryptionFactory;
    static double _rateLimit;
    static double _rateBurst;

    // Heartbeat
    static uint32_t _hbInterval;
    static uint32_t _hbTimeout;
    static std::function<void(ISession *)> _hbPingFunc;

    // Server Role
    static ServerRole _serverRole;
};

} // namespace System
