#include "System/Session/SessionFactory.h"
#include "Session.h"
#include "System/Pch.h"
#include "System/Session/SessionPool.h"

namespace System {

std::atomic<uint64_t> SessionFactory::_nextSessionId = 1;
std::function<std::unique_ptr<IPacketEncryption>()> SessionFactory::_encryptionFactory;
double SessionFactory::_rateLimit = 100.0;
double SessionFactory::_rateBurst = 200.0;

Session *SessionFactory::CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, IDispatcher *dispatcher)
{
    // Use SessionPool (Zero Contention T*)
    Session *session = SessionPool<Session>::Acquire();
    if (session == nullptr)
    {
        // Handle pool exhaustion
        return nullptr;
    }

    uint64_t id = _nextSessionId.fetch_add(1);
    session->Reset(socket, id, dispatcher);

    // [Encryption] Inject if factory is set
    if (_encryptionFactory)
    {
        session->SetEncryption(_encryptionFactory());
    }

    return session;
}

void SessionFactory::Destroy(Session *session)
{
    if (session)
    {
        SessionPool<Session>::Release(session);
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

} // namespace System
