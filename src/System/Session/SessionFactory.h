#pragma once

#include "System/Network/IPacketEncryption.h"
#include "System/Pch.h"
#include "System/Session/Session.h"
#include <atomic>
#include <functional>

namespace System {

class IDispatcher;

class SessionFactory
{
public:
    // [Updated] Use SessionPool and Raw Pointers for Zero Overhead
    static Session *CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, IDispatcher *dispatcher);

    // [Restored] Required by DispatcherImpl
    static void Destroy(Session *session);

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

private:
    static std::atomic<uint64_t> _nextSessionId;
    static std::function<std::unique_ptr<IPacketEncryption>()> _encryptionFactory;
    static double _rateLimit;
    static double _rateBurst;
};

} // namespace System
