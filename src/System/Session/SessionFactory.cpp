#include "System/Session/SessionFactory.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include "System/Session/BackendSession.h"
#include "System/Session/GatewaySession.h"
#include "System/Session/SessionPool.h"

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

    // [Hot Path Optimization] No branching after initialization
    // Factory selects session type based on server role (set once at startup)
    if (_serverRole == ServerRole::Gateway)
    {
        // Gateway: Encryption-enabled session
        GatewaySession *gatewaySess = SessionPool<GatewaySession>::Acquire();
        if (!gatewaySess)
        {
            std::cout << "[DEBUG] SessionPool<GatewaySession>::Acquire() returned NULL!" << std::endl;
            return nullptr;
        }

        std::cout << "[DEBUG] GatewaySession Acquired. Resetting..." << std::endl;
        gatewaySess->Reset(std::static_pointer_cast<void>(socket), id, dispatcher);

        // Inject encryption (mandatory for Gateway)
        if (_encryptionFactory)
        {
            std::cout << "[DEBUG] Setting Encryption..." << std::endl;
            gatewaySess->SetEncryption(_encryptionFactory());
        }
        else
        {
            std::cout << "[DEBUG] WARNING: No EncryptionFactory set!" << std::endl;
        }

        // Apply heartbeat if configured
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
    else // ServerRole::Backend
    {
        // Backend: Zero-copy optimized session (no encryption)
        BackendSession *backendSess = SessionPool<BackendSession>::Acquire();
        if (!backendSess)
            return nullptr;

        backendSess->Reset(std::static_pointer_cast<void>(socket), id, dispatcher);

        // Apply heartbeat if configured
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

void SessionFactory::Destroy(ISession *session)
{
    if (!session)
        return;

    // Determine session type and release to appropriate pool
    if (_serverRole == ServerRole::Gateway)
    {
        SessionPool<GatewaySession>::Release(static_cast<GatewaySession *>(session));
    }
    else
    {
        SessionPool<BackendSession>::Release(static_cast<BackendSession *>(session));
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
