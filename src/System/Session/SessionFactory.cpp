#include "System/Session/SessionFactory.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include "System/Session/BackendSession.h"
#include "System/Session/GatewaySession.h"
#include "System/Session/SessionPool.h"
#include "System/Session/UDPSession.h"

namespace System {

std::atomic<uint64_t> SessionFactory::_nextSessionId = 1;
std::function<std::unique_ptr<IPacketEncryption>()> SessionFactory::_encryptionFactory;
double SessionFactory::_rateLimit = 10000.0;
double SessionFactory::_rateBurst = 20000.0;

// Heartbeat Defaults
uint32_t SessionFactory::_hbInterval = 0;
uint32_t SessionFactory::_hbTimeout = 0;
std::function<void(ISession *)> SessionFactory::_hbPingFunc = nullptr;

// Server Role Default (Gateway for backward compatibility)
ServerRole SessionFactory::_serverRole = ServerRole::Gateway;

ISession *SessionFactory::CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, IDispatcher *dispatcher)
{
    uint64_t id = _nextSessionId.fetch_add(1);

    if (_serverRole == ServerRole::Gateway)
    {
        auto &pool = GetSessionPool<GatewaySession>();
        GatewaySession *gatewaySess = pool.Acquire();
        if (!gatewaySess)
        {
            std::cout << "[DEBUG] GetSessionPool<GatewaySession>().Acquire() returned NULL!" << std::endl;
            return nullptr;
        }

        std::cout << "[DEBUG] GatewaySession Acquired. Resetting..." << std::endl;
        gatewaySess->Reset(std::static_pointer_cast<void>(socket), id, dispatcher);

        if (_encryptionFactory)
        {
            std::cout << "[DEBUG] Setting Encryption..." << std::endl;
            gatewaySess->SetEncryption(_encryptionFactory());
        }
        else
        {
            std::cout << "[DEBUG] WARNING: No EncryptionFactory set!" << std::endl;
        }

        if (_hbInterval > 0)
        {
            gatewaySess->ConfigHeartbeat(
                _hbInterval,
                _hbTimeout,
                [](GatewaySession *s)
                {
                    if (_hbPingFunc)
                        _hbPingFunc(static_cast<ISession *>(s));
                }
            );
        }

        return static_cast<ISession *>(gatewaySess);
    }
    else
    {
        auto &pool = GetSessionPool<BackendSession>();
        BackendSession *backendSess = pool.Acquire();
        if (!backendSess)
            return nullptr;

        backendSess->Reset(std::static_pointer_cast<void>(socket), id, dispatcher);

        if (_hbInterval > 0)
        {
            backendSess->ConfigHeartbeat(
                _hbInterval,
                _hbTimeout,
                [](BackendSession *s)
                {
                    if (_hbPingFunc)
                        _hbPingFunc(static_cast<ISession *>(s));
                }
            );
        }

        return static_cast<ISession *>(backendSess);
    }
}

ISession *SessionFactory::CreateUDPSession(const boost::asio::ip::udp::endpoint &endpoint, IDispatcher *dispatcher)
{
    uint64_t id = _nextSessionId.fetch_add(1);

    auto &pool = GetSessionPool<UDPSession>();
    UDPSession *udpSess = pool.Acquire();
    if (!udpSess)
    {
        LOG_ERROR("[SessionFactory] GetSessionPool<UDPSession>().Acquire() returned NULL!");
        // Fallback or return nullptr. For now, let's try to allocate one if pool is empty/failed
        // But SessionPool checks queue. If empty, it returns nullptr.
        // We probably should handle this better, but for now consistent with GatewaySession logic.
        return nullptr;
    }

    // Create a null shared_ptr for socket (UDP uses endpoint, not socket)
    std::shared_ptr<void> socketPtr = nullptr;

    udpSess->Reset(socketPtr, id, dispatcher, endpoint);
    udpSess->OnConnect();

    return static_cast<ISession *>(udpSess);
}

void SessionFactory::Destroy(ISession *session)
{
    if (!session)
        return;

    session->OnRecycle();

    // [UDP] Check if this is a UDP session first
    if (auto *udpSession = dynamic_cast<UDPSession *>(session))
    {
        GetSessionPool<UDPSession>().Release(udpSession);
        return;
    }

    // [TCP] Handle TCP sessions based on server role
    if (_serverRole == ServerRole::Gateway)
    {
        auto &pool = GetSessionPool<GatewaySession>();
        pool.Release(static_cast<GatewaySession *>(session));
    }
    else
    {
        auto &pool = GetSessionPool<BackendSession>();
        pool.Release(static_cast<BackendSession *>(session));
    }
}

void SessionFactory::SetEncryptionFactory(std::function<std::unique_ptr<IPacketEncryption>()> factory)
{
    _encryptionFactory = factory;
}

void SessionFactory::SetRateLimitConfig(double rate, double burst)
{
    _rateLimit = rate;
    _rateBurst = burst;
}

void SessionFactory::SetHeartbeatConfig(
    uint32_t intervalMs, uint32_t timeoutMs, std::function<void(ISession *)> pingFunc
)
{
    _hbInterval = intervalMs;
    _hbTimeout = timeoutMs;
    _hbPingFunc = pingFunc;
}

void SessionFactory::SetServerRole(ServerRole role)
{
    _serverRole = role;
}

} // namespace System
