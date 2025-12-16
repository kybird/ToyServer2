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
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        _acceptor.open(endpoint.protocol());
        _acceptor.set_option(boost::asio::socket_base::reuse_address(false));
        _acceptor.bind(endpoint);
        _acceptor.listen();

        LOG_INFO("Network listening on port {}", port);

        StartAccept();
        return true;
    } catch (const std::exception &e)
    {
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
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(_ioContext);

    _acceptor.async_accept(
        *socket,
        [this, socket](const boost::system::error_code &error)
        {
            if (!error)
            {
                // Create Session via Factory
                auto session = SessionFactory::CreateSession(socket, _dispatcher);
                if (session)
                {
                    session->OnConnect();
                }
                else
                {
                    LOG_ERROR("Session Creation Failed (Pool Exhausted)");
                    socket->close();
                }
            }
            else
            {
                LOG_ERROR("Accept failed: {}", error.message());
            }

            StartAccept(); // Loop
        }
    );
}

} // namespace System
