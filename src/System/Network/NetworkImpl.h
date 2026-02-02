#pragma once

#include "System/Network/INetwork.h"
#include "System/Pch.h"

namespace System {

class IDispatcher;
class UDPNetworkImpl;

class NetworkImpl : public INetwork
{
public:
    NetworkImpl();
    virtual ~NetworkImpl();

    bool Start(uint16_t port) override;
    void Stop() override;
    void Run() override;

    boost::asio::io_context &GetIOContext()
    {
        return _ioContext;
    }

    void SetDispatcher(IDispatcher *dispatcher)
    {
        _dispatcher = dispatcher;
    }

private:
    void StartAccept();

private:
    boost::asio::io_context _ioContext;
    boost::asio::ip::tcp::acceptor _acceptor;
    UDPNetworkImpl *_udpNetwork = nullptr;
    IDispatcher *_dispatcher = nullptr;
    std::atomic<bool> _isStopping{false};
};

} // namespace System
