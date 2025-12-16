#include "System/Session/SessionFactory.h"
#include "Session.h"
#include "System/Pch.h"
#include "System/Session/SessionPool.h"

namespace System {

std::atomic<uint64_t> SessionFactory::_nextSessionId = 1;

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

    return session;
}

void SessionFactory::Destroy(Session *session)
{
    if (session)
    {
        SessionPool<Session>::Release(session);
    }
}

} // namespace System
