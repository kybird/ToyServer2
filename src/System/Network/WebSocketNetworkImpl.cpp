#include "System/Network/WebSocketNetworkImpl.h"
#include "System/ILog.h"
#include "System/Network/WebSocketSession.h"
#include "System/Utility/Encoding.h"

namespace System {

WebSocketNetworkImpl::WebSocketNetworkImpl(boost::asio::io_context &ioc) : _ioc(ioc)
{
}

WebSocketNetworkImpl::~WebSocketNetworkImpl()
{
    Stop();
}

bool WebSocketNetworkImpl::Start(uint16_t port)
{
    _isStopping.store(false);

    try
    {
        auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port);

        _acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(_ioc);
        _acceptor->open(endpoint.protocol());
        _acceptor->set_option(boost::asio::socket_base::reuse_address(true));
        _acceptor->bind(endpoint);
        _acceptor->listen();

        LOG_INFO("WebSocket Server listening on 127.0.0.1:{}", port);

        DoAccept();
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("WebSocket Server Start Failed: {}", e.what());
        return false;
    }
}

void WebSocketNetworkImpl::Stop()
{
    _isStopping.store(true);

    if (_acceptor && _acceptor->is_open())
    {
        boost::system::error_code ec;
        _acceptor->close(ec);
    }

    std::lock_guard<std::mutex> lock(_sessionsMutex);
    _sessions.clear();
}

void WebSocketNetworkImpl::Broadcast(const std::string &message)
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);

    // [Dead Session Cleanup] 연결 끝긴 세션 제거
    _sessions.erase(
        std::remove_if(
            _sessions.begin(),
            _sessions.end(),
            [](const std::shared_ptr<WebSocketSession> &session)
            {
                // use_count == 1이면 이 벡터만 참조 중 (연결 끝남)
                return session.use_count() == 1;
            }
        ),
        _sessions.end()
    );

    for (auto &session : _sessions)
    {
        session->Send(message);
    }
}

void WebSocketNetworkImpl::BroadcastBinary(const uint8_t *data, size_t length)
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);

    // [Dead Session Cleanup] 연결 끝긴 세션 제거
    _sessions.erase(
        std::remove_if(
            _sessions.begin(),
            _sessions.end(),
            [](const std::shared_ptr<WebSocketSession> &session)
            {
                return session.use_count() == 1;
            }
        ),
        _sessions.end()
    );

    for (auto &session : _sessions)
    {
        session->SendBinary(data, length);
    }
}

void WebSocketNetworkImpl::DoAccept()
{
    if (!_acceptor || _isStopping.load())
        return;

    _acceptor->async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
        {
            OnAccept(ec, std::move(socket));
        }
    );
}

void WebSocketNetworkImpl::OnAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
{
    if (_isStopping.load())
        return;

    if (ec)
    {
        LOG_ERROR("WebSocket Accept Error: {}", Utility::ToUtf8(ec.message()));
    }
    else
    {
        LOG_INFO("WebSocket Client Connected: {}", socket.remote_endpoint().address().to_string());

        auto session = std::make_shared<WebSocketSession>(std::move(socket));

        {
            std::lock_guard<std::mutex> lock(_sessionsMutex);
            _sessions.push_back(session);
        }

        session->Run();
    }

    DoAccept();
}

} // namespace System
