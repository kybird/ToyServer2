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

void UDPEndpointRegistry::RegisterWithToken(const boost::asio::ip::udp::endpoint &endpoint,
                                             ISession *session, uint128_t udpToken)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _sessions.find(endpoint);
    if (it != _sessions.end())
    {
        it->second.session = session;
        it->second.udpToken = udpToken;
        it->second.lastActivity = std::chrono::steady_clock::now();
    }
    else
    {
        SessionInfo info;
        info.session = session;
        info.udpToken = udpToken;
        info.lastActivity = std::chrono::steady_clock::now();
        _sessions[endpoint] = info;
    }

    _tokens[udpToken] = endpoint;
}

ISession *UDPEndpointRegistry::GetEndpointByToken(uint128_t token)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto tokenIt = _tokens.find(token);
    if (tokenIt != _tokens.end())
    {
        auto sessionIt = _sessions.find(tokenIt->second);
        if (sessionIt != _sessions.end())
        {
            return sessionIt->second.session;
        }
    }
    return nullptr;
}

size_t UDPEndpointRegistry::CleanupTimeouts(uint32_t timeoutMs)
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t removed = 0;
    auto now = std::chrono::steady_clock::now();

    auto it = _sessions.begin();
    while (it != _sessions.end())
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.lastActivity
        ).count();

        if (elapsed > timeoutMs)
        {
            _tokens.erase(it->second.udpToken);
            it = _sessions.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }
    return removed;
}

} // namespace System
