#include "System/Network/UDPEndpointRegistry.h"
#include "System/ISession.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include <algorithm>

namespace System {

UDPEndpointRegistry::UDPEndpointRegistry()
{
}

UDPEndpointRegistry::~UDPEndpointRegistry()
{
}

void UDPEndpointRegistry::Register(const boost::asio::ip::udp::endpoint &endpoint, ISession *session)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _sessions.find(endpoint);
    if (it != _sessions.end())
    {
        // Update existing entry
        it->second.session = session;
        it->second.lastActivity = std::chrono::steady_clock::now();
    }
    else
    {
        // New entry
        SessionInfo info;
        info.session = session;
        info.lastActivity = std::chrono::steady_clock::now();
        _sessions[endpoint] = info;
    }
}

ISession *UDPEndpointRegistry::Find(const boost::asio::ip::udp::endpoint &endpoint)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _sessions.find(endpoint);
    if (it != _sessions.end())
    {
        return it->second.session;
    }
    return nullptr;
}

void UDPEndpointRegistry::Remove(const boost::asio::ip::udp::endpoint &endpoint)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _sessions.erase(endpoint);
}

void UDPEndpointRegistry::UpdateActivity(const boost::asio::ip::udp::endpoint &endpoint)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _sessions.find(endpoint);
    if (it != _sessions.end())
    {
        it->second.lastActivity = std::chrono::steady_clock::now();
    }
}

size_t UDPEndpointRegistry::CleanupTimeouts(uint32_t timeoutMs)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto now = std::chrono::steady_clock::now();
    auto timeoutThreshold = std::chrono::milliseconds(timeoutMs);
    size_t removedCount = 0;

    for (auto it = _sessions.begin(); it != _sessions.end();)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastActivity);

        if (elapsed >= timeoutThreshold)
        {
            LOG_INFO("[UDPEndpointRegistry] Session {}:{} timed out ({}ms inactive)",
                     it->first.address().to_string(), it->first.port(), elapsed.count());

            it = _sessions.erase(it);
            removedCount++;
        }
        else
        {
            ++it;
        }
    }

    return removedCount;
}

} // namespace System
