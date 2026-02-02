#pragma once

#include "System/Pch.h"
#include <memory>
#include <mutex>
#include <vector>

namespace System {

class WebSocketSession;

/**
 * @brief WebSocket 서버 구현 (Private Implementation)
 * @note Application 코드에서 직접 include 금지!
 */
class WebSocketNetworkImpl
{
public:
    explicit WebSocketNetworkImpl(boost::asio::io_context &ioc);
    ~WebSocketNetworkImpl();

    bool Start(uint16_t port);
    void Stop();

    // 모든 연결된 클라이언트에 브로드캐스트
    void Broadcast(const std::string &message);
    void BroadcastBinary(const uint8_t *data, size_t length);

private:
    void DoAccept();
    void OnAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);

    boost::asio::io_context &_ioc;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> _acceptor;
    std::vector<std::shared_ptr<WebSocketSession>> _sessions;
    std::mutex _sessionsMutex;
    std::atomic<bool> _isStopping{false};
};

} // namespace System
