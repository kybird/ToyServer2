#include "System/Network/NetworkImpl.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include "System/Session/SessionFactory.h"

namespace System {

NetworkImpl::NetworkImpl() : _acceptor(_ioContext)
{
}

NetworkImpl::~NetworkImpl()
{
    Stop();
}

bool NetworkImpl::Start(uint16_t port)
{
    try
    {
        // Bind explicitly to 127.0.0.1 to avoid ambiguities
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), port);

        _acceptor.open(endpoint.protocol());
        // [Debugging] Disable reuse_address to detect zombie processes
        _acceptor.set_option(boost::asio::socket_base::reuse_address(false));
        _acceptor.bind(endpoint);
        LOG_ERROR("[DEBUG] Bound to 127.0.0.1:{}", port);
        _acceptor.listen();
        LOG_ERROR("[DEBUG] Listening... Calling StartAccept()");

        LOG_INFO("Network listening on port {}", port);

        StartAccept();

        // [DEBUG] IO Context Liveness Probe
        auto timer = std::make_shared<boost::asio::steady_timer>(_ioContext, std::chrono::seconds(1));
        timer->async_wait(
            [timer, this](const boost::system::error_code &ec)
            {
                if (!ec)
                {
                    LOG_ERROR("[DEBUG] IO Context IS ALIVE. Acceptor Open: {}", _acceptor.is_open());
                }
            }
        );

        return true;
    } catch (const std::exception &e)
    {
        std::cerr << "[DEBUG] Network Start Failed on Port " << port << ": " << e.what() << std::endl;
        LOG_ERROR("Network Start Failed: {}", e.what());
        return false;
    }
}

void NetworkImpl::Stop()
{
    if (!_ioContext.stopped())
    {
        _ioContext.stop();
    }
}

void NetworkImpl::Run()
{
    _ioContext.run();
}

void NetworkImpl::StartAccept()
{
    LOG_ERROR("[DEBUG] StartAccept() Called. Posting async_accept...");
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(_ioContext);

    _acceptor.async_accept(
        *socket,
        [this, socket](const boost::system::error_code &error)
        {
            if (!error)
            {
                LOG_ERROR("[DEBUG] Accept Handler: Connected from {}", socket->remote_endpoint().address().to_string());
                auto session = SessionFactory::CreateSession(socket, _dispatcher);
                if (session)
                    session->OnConnect();
            }
            else
            {
                LOG_ERROR("[DEBUG] Accept Error: {}", error.message());
            }

            StartAccept();
        }
    );
}

} // namespace System
