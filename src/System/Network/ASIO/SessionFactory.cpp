#include "System/Network/ASIO/SessionFactory.h"
#include "System/Pch.h"

namespace System {

std::atomic<uint64_t> SessionFactory::_nextSessionId = 1;
std::mutex SessionFactory::_sessionMapLock;
std::unordered_map<uint64_t, std::shared_ptr<AsioSession>> SessionFactory::_sessionMap;

std::shared_ptr<AsioSession>
SessionFactory::CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, IDispatcher *dispatcher)
{
    auto session = ObjectPool<AsioSession>::Acquire();
    uint64_t id = _nextSessionId++;
    session->Reset(socket, id, dispatcher);

    {
        std::lock_guard<std::mutex> lock(_sessionMapLock);
        _sessionMap[id] = session;
    }

    return session;
}

std::shared_ptr<AsioSession> SessionFactory::FindSession(uint64_t id)
{
    std::lock_guard<std::mutex> lock(_sessionMapLock);
    auto it = _sessionMap.find(id);
    if (it != _sessionMap.end())
    {
        return it->second;
    }
    return nullptr;
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
  // namespace System
