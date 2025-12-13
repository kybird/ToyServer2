#include "System/Network/ASIO/SessionFactory.h"
#include "AsioSession.h"
#include "SessionPool.h"
#include "System/Pch.h"
namespace System {

std::atomic<uint64_t> SessionFactory::_nextSessionId = 1;
std::mutex SessionFactory::_sessionMapLock;
std::unordered_map<uint64_t, AsioSession *> SessionFactory::_sessionMap;

AsioSession *
SessionFactory::CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, IDispatcher *dispatcher)
{
    // Use SessionPool (Zero Contention T*)
    // Use SessionPool (Zero Contention T*)
    AsioSession *session = SessionPool<AsioSession>::Acquire();
    if (session == nullptr)
    {
        // Handle pool exhaustion
        return nullptr;
    }

    uint64_t id = _nextSessionId.fetch_add(1);
    session->Reset(socket, id, dispatcher);

    {
        std::lock_guard<std::mutex> lock(_sessionMapLock);
        _sessionMap[id] = session;
    }

    return session;
}

AsioSession *SessionFactory::FindSession(uint64_t id)
{
    std::lock_guard<std::mutex> lock(_sessionMapLock);
    auto it = _sessionMap.find(id);
    if (it != _sessionMap.end())
    {
        return it->second;
    }
    return nullptr;
}

void SessionFactory::Destroy(AsioSession *session)
{
    if (session)
    {
        // Remove from map if present (Dispatcher calls Destroy usually after RemoveSession, but safe to check)
        // Actually Destroy is called AFTER RemoveSession in Dispatcher logic,
        // but let's ensure we don't leak map entries if called directly.
        // But removing from map requires ID. session->GetId().
        RemoveSession(session->GetId());

        SessionPool<AsioSession>::Release(session);
    }
}

void SessionFactory::RemoveSession(uint64_t id)
{
    std::lock_guard<std::mutex> lock(_sessionMapLock);
    _sessionMap.erase(id);
}

size_t SessionFactory::GetSessionCount()
{
    std::lock_guard<std::mutex> lock(_sessionMapLock);
    return _sessionMap.size();
}

} // namespace System
