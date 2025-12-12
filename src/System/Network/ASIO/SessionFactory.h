#pragma once

#include "System/Memory/ObjectPool.h"
#include "System/Network/ASIO/AsioSession.h"
#include "System/Pch.h"
#include <mutex>
#include <unordered_map>

namespace System {

class IDispatcher;

class SessionFactory {
public:
    // Use ObjectPool to get a session
    static std::shared_ptr<AsioSession> CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                                      IDispatcher *dispatcher);
    static std::shared_ptr<AsioSession> FindSession(uint64_t id);
    static void RemoveSession(uint64_t id);

private:
    static std::atomic<uint64_t> _nextSessionId;
    static std::mutex _sessionMapLock;
    static std::unordered_map<uint64_t, std::shared_ptr<AsioSession>> _sessionMap;
};

} // namespace System
