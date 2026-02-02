#pragma once

#include "System/Pch.h"
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;

namespace System {

/**
 * @brief 개별 WebSocket 클라이언트 연결 세션
 */
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
public:
    explicit WebSocketSession(boost::asio::ip::tcp::socket socket);
    ~WebSocketSession();

    void Run();
    void Send(const std::string &message);
    void SendBinary(const uint8_t *data, size_t length);
    void Close();

private:
    void DoRead();
    void OnRead(beast::error_code ec, std::size_t bytes_transferred);
    void OnWrite(beast::error_code ec, std::size_t bytes_transferred);

    websocket::stream<boost::asio::ip::tcp::socket> _ws;
    beast::flat_buffer _buffer;
    std::mutex _writeMutex;
    std::queue<std::string> _sendQueue;
    bool _isWriting = false; // DoWrite 진행 중 플래그
    void DoWrite();
};

} // namespace System
