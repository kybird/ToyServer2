#pragma once

#include "System/Network/ASIO/AsioSession.h"
#include "System/Network/ASIO/SessionPool.h"
#include "System/Pch.h"
#include <mutex>
#include <unordered_map>

namespace System {

class IDispatcher;

class SessionFactory
{
public:
    // [Updated] Use SessionPool and Raw Pointers for Zero Overhead
    static AsioSession *CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, IDispatcher *dispatcher);
    static AsioSession *FindSession(uint64_t id);

    // [Restored] Required by DispatcherImpl
    static void Destroy(AsioSession *session);

    static void RemoveSession(uint64_t id);
    static size_t GetSessionCount();

private:
    static std::atomic<uint64_t> _nextSessionId;
    static std::mutex _sessionMapLock;
    static std::unordered_map<uint64_t, AsioSession *> _sessionMap;
};

} // namespace System
