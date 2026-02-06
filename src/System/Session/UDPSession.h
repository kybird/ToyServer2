#pragma once

#include "System/Session/Session.h"
#include "System/Pch.h"
#include <boost/asio/ip/udp.hpp>
#include <memory>

namespace System {

class IDispatcher;
class UDPNetworkImpl;

struct UDPSessionImpl;

class UDPSession : public Session
{
    friend struct UDPSessionImpl;

public:
    UDPSession();
    virtual ~UDPSession();

    void Reset(std::shared_ptr<void> socketVoidPtr, uint64_t sessionId, IDispatcher *dispatcher,
              const boost::asio::ip::udp::endpoint &endpoint);

    const boost::asio::ip::udp::endpoint &GetEndpoint() const;

    void UpdateActivity();

    const std::chrono::steady_clock::time_point &GetLastActivity() const;

    void HandleData(const uint8_t *data, size_t length);

    void SetNetwork(UDPNetworkImpl *network);
    UDPNetworkImpl *GetNetwork() const;

    void SetUdpToken(uint128_t token);
    uint128_t GetUdpToken() const;

    void OnConnect() override;
    void OnDisconnect() override;
    void Close() override;

protected:
    void Flush() override;

private:
    std::unique_ptr<UDPSessionImpl> _impl;
};

} // namespace System
