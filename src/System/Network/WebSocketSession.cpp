#include "System/Network/WebSocketSession.h"
#include "System/ILog.h"
#include "System/Utility/Encoding.h"

namespace System {

WebSocketSession::WebSocketSession(boost::asio::ip::tcp::socket socket) : _ws(std::move(socket))
{
}

WebSocketSession::~WebSocketSession()
{
    boost::system::error_code ec;
    if (_ws.is_open())
    {
        _ws.close(websocket::close_code::normal, ec);
    }
}

void WebSocketSession::Close()
{
    boost::system::error_code ec;
    if (_ws.is_open())
    {
        _ws.close(websocket::close_code::normal, ec);
    }
}

void WebSocketSession::Run()
{
    // WebSocket 핸드셰이크 수락
    _ws.async_accept(
        [self = shared_from_this()](beast::error_code ec)
        {
            if (ec)
            {
                LOG_ERROR("WebSocket Handshake Failed: {}", Utility::ToUtf8(ec.message()));
                return;
            }

            LOG_INFO("WebSocket Handshake Complete");
            self->DoRead();
        }
    );
}

void WebSocketSession::Send(const std::string &message)
{
    std::lock_guard<std::mutex> lock(_writeMutex);
    _sendQueue.push(message);

    // 현재 전송 중이 아니면 즉시 전송 시작
    if (!_isWriting)
    {
        DoWrite();
    }
}

void WebSocketSession::SendBinary(const uint8_t *data, size_t length)
{
    // 바이너리 데이터를 string으로 변환하여 기존 큐 활용
    std::string binaryStr(reinterpret_cast<const char *>(data), length);

    std::lock_guard<std::mutex> lock(_writeMutex);
    _sendQueue.push(std::move(binaryStr));

    // 현재 전송 중이 아니면 즉시 전송 시작
    if (!_isWriting)
    {
        DoWrite();
    }
}

void WebSocketSession::DoWrite()
{
    // [락 범위] _writeMutex는 호출자(Send/SendBinary/OnWrite)가 이미 획득한 상태
    if (_sendQueue.empty())
    {
        _isWriting = false;
        return;
    }

    _isWriting = true;
    _ws.text(true); // 텍스트 모드로 전송 (JSON 호환)
    _ws.async_write(
        boost::asio::buffer(_sendQueue.front()),
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes)
        {
            self->OnWrite(ec, bytes);
        }
    );
}

// Re-implementing correctly below:

void WebSocketSession::DoRead()
{
    _ws.async_read(
        _buffer,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes)
        {
            self->OnRead(ec, bytes);
        }
    );
}

void WebSocketSession::OnRead(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec == websocket::error::closed)
    {
        LOG_INFO("WebSocket Client Disconnected (Clean Close)");
        return;
    }

    if (ec == boost::asio::error::connection_reset || ec == boost::asio::error::eof ||
        ec == boost::asio::error::broken_pipe || ec == boost::asio::error::connection_aborted)
    {
        LOG_INFO("WebSocket Client Disconnected (Connection Lost: {})", Utility::ToUtf8(ec.message()));
        return;
    }

    if (ec)
    {
        LOG_ERROR("WebSocket Read Error: {} ({})", Utility::ToUtf8(ec.message()), ec.value());
        return;
    }

    // Echo back for now (디버그용)
    std::string received = beast::buffers_to_string(_buffer.data());
    LOG_INFO("WebSocket Received: {}", received);

    _buffer.consume(_buffer.size());
    DoRead();
}

void WebSocketSession::OnWrite(beast::error_code ec, std::size_t bytes_transferred)
{
    std::lock_guard<std::mutex> lock(_writeMutex);

    // 전송 완료된 항목 제거
    if (!_sendQueue.empty())
    {
        _sendQueue.pop();
    }

    if (ec)
    {
        // 에러 발생 시 전송 중단
        _isWriting = false;

        if (ec == boost::asio::error::connection_reset || ec == boost::asio::error::broken_pipe)
        {
            LOG_INFO("WebSocket Write Failed (Connection Lost): {}", Utility::ToUtf8(ec.message()));
        }
        else
        {
            LOG_ERROR("WebSocket Write Error: {}", Utility::ToUtf8(ec.message()));
        }
        return;
    }

    // 다음 항목 전송
    DoWrite();
}

} // namespace System
