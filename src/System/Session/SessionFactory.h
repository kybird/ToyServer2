#pragma once

#include "System/Pch.h"
#include "System/Session/Session.h"
#include <atomic>

namespace System {

class IDispatcher;

class SessionFactory
{
public:
    // [Updated] Use SessionPool and Raw Pointers for Zero Overhead
    static Session *CreateSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, IDispatcher *dispatcher);

    // [Restored] Required by DispatcherImpl
    static void Destroy(Session *session);

private:
    static std::atomic<uint64_t> _nextSessionId;
};

} // namespace System
