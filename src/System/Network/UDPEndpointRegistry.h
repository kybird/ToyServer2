#pragma once

#include "System/Pch.h"
#include "System/Network/UDPEndpointRegistry.h"
#include "System/Session/UDPSession.h"
#include <chrono>
#include <cstdint>

namespace System {

struct SessionInfo
{
    ISession *session;
    std::chrono::steady_clock::time_point lastActivity;
    uint128_t udpToken = 0; // 16바이트 랜덤 토큰
};

class UDPEndpointRegistry
{
public:
    UDPEndpointRegistry();
    ~UDPEndpointRegistry();

    void Register(const boost::asio::ip::udp::endpoint &endpoint, ISession *session);
    ISession *Find(const boost::asio::ip::udp::endpoint &endpoint);
    void Remove(const boost::asio::ip::udp::endpoint &endpoint);

    // [New] 토큰 기반 세션 등록 및 조회
    void RegisterWithToken(const boost::asio::ip::udp::endpoint &endpoint, ISession *session, uint128_t udpToken);
    ISession *GetEndpointByToken(uint128_t token);
    void UpdateActivity(const boost::asio::ip::udp::endpoint &endpoint);

    size_t CleanupTimeouts(uint32_t timeoutMs);

private:
    std::unordered_map<boost::asio::ip::udp::endpoint, SessionInfo> _sessions;
    std::unordered_map<uint128_t, boost::asio::ip::udp::endpoint, System::uint128_hash> _tokens;
    std::mutex _mutex;
};

} // namespace System
