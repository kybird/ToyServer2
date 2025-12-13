#include "System/Network/ASIO/AsioSession.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/ILog.h"
#include "System/Memory/PacketPool.h"
#include "System/Pch.h"

namespace System {

AsioSession::AsioSession()
{
}
AsioSession::~AsioSession()
{
    LOG_INFO("Session Destroyed: ID {}", _id);
}

// Pool reuse
void AsioSession::Reset()
{
    _id = 0;
    _connected.store(false, std::memory_order_relaxed);
    _gracefulShutdown.store(false, std::memory_order_relaxed);
    _ioRef.store(0, std::memory_order_relaxed);
    _socket.reset();
    _recvBuffer.Reset();
    _flowControlTimer.reset();
}

void AsioSession::OnRecycle()
{
    if (_socket && _socket->is_open())
        Close();
    _socket.reset();
    _dispatcher = nullptr;
}

void AsioSession::Reset(
    std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint64_t sessionId, IDispatcher *dispatcher
)
{
    _socket = socket;
    _id = sessionId;
    _dispatcher = dispatcher;

    _connected.store(false, std::memory_order_relaxed);
    _gracefulShutdown.store(false, std::memory_order_relaxed);
    _ioRef.store(0, std::memory_order_relaxed);

    if (_socket && _socket->is_open())
    {
        boost::asio::ip::tcp::no_delay noDelay(true);
        boost::system::error_code ec;
        _socket->set_option(noDelay, ec);

        boost::asio::socket_base::receive_buffer_size recvOption(4 * 1024 * 1024);
        _socket->set_option(recvOption, ec);

        boost::asio::socket_base::send_buffer_size sendOption(4 * 1024 * 1024);
        _socket->set_option(sendOption, ec);
    }

    _reader.Init(_socket, this);
    _writer.Init(_socket, this);
    _recvBuffer.Reset();
    _flowControlTimer.reset();
}

void AsioSession::GracefulClose()
{
    if (!_gracefulShutdown.exchange(true))
    {
        // Writer will handle flush/drain
    }
}

void AsioSession::OnConnect()
{
    LOG_INFO("Session Connected: ID {}", _id);
    _connected.store(true, std::memory_order_relaxed);

    SystemMessage msg;
    msg.type = MessageType::NETWORK_CONNECT;
    msg.sessionId = _id;
    msg.session = this;
    if (_dispatcher)
        _dispatcher->Post(msg);

    RegisterRecv();
}

void AsioSession::OnDisconnect()
{
    if (_connected.exchange(false))
    {
        SystemMessage msg;
        msg.type = MessageType::NETWORK_DISCONNECT;
        msg.sessionId = _id;
        msg.session = this;
        if (_dispatcher)
            _dispatcher->Post(msg);
    }
}

void AsioSession::Send(std::span<const uint8_t> data)
{
    if (!_connected.load(std::memory_order_relaxed))
        return;

    auto packet = PacketPool::Allocate(data.size());
    packet->assign(data.data(), data.data() + data.size());
    _writer.Send(std::move(packet));
}

void AsioSession::Send(boost::intrusive_ptr<Packet> packet)
{
    if (!_connected.load(std::memory_order_relaxed))
        return;
    _writer.Send(std::move(packet));
}

void AsioSession::Close()
{
    if (_socket && _socket->is_open())
    {
        boost::system::error_code ec;
        _socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        _socket->close(ec);
    }
    _writer.Clear();
    OnDisconnect();
}

void AsioSession::RegisterRecv()
{
    if (_socket && _socket->is_open())
    {
        _recvBuffer.Clean();
        _reader.ReadSome(_recvBuffer.WritePos(), _recvBuffer.FreeSize());
    }
}

void AsioSession::OnRead(size_t bytesTransferred)
{
    if (!_recvBuffer.OnWrite(bytesTransferred))
    {
        LOG_ERROR("Session {} Buffer Overflow", _id);
        Close();
        return;
    }

    while (true)
    {
        int32_t dataSize = _recvBuffer.DataSize();
        if (dataSize < sizeof(Share::PacketHeader))
            break;

        Share::PacketHeader *header = reinterpret_cast<Share::PacketHeader *>(_recvBuffer.ReadPos());
        if (header->size > 1024 * 10)
        {
            LOG_ERROR("Session {} Invalid Packet Size: {}", _id, header->size);
            Close();
            return;
        }

        if (dataSize < header->size)
            break;

        SystemMessage msg;
        msg.type = MessageType::NETWORK_DATA;
        msg.sessionId = _id;
        msg.session = this;

        auto packetData = PacketPool::Allocate(header->size);
        packetData->assign(_recvBuffer.ReadPos(), _recvBuffer.ReadPos() + header->size);
        msg.data = std::move(packetData);

        if (_dispatcher)
            _dispatcher->Post(msg);

        _recvBuffer.OnRead(header->size);

        // [Flow Control]
        if (_dispatcher && _dispatcher->IsOverloaded())
        {
            // LOG_WARN("Session {} Overloaded. Pausing Read.", _id);

            // Pause Reading: Schedule Resume after 10ms
            if (!_flowControlTimer)
            {
                // Create timer associated with socket's executor (thread-safe)
                _flowControlTimer = std::make_unique<boost::asio::steady_timer>(_socket->get_executor());
            }

            _flowControlTimer->expires_after(std::chrono::milliseconds(10));
            _flowControlTimer->async_wait(
                [this](const boost::system::error_code &ec)
                {
                    OnResumeRead(ec);
                }
            );
            return; // Stop Reading (TCP Back-pressure)
        }
    }

    RegisterRecv();
}

void AsioSession::OnResumeRead(const boost::system::error_code &ec)
{
    if (ec || !_connected.load(std::memory_order_relaxed))
        return;

    if (_dispatcher && _dispatcher->IsOverloaded())
    {
        // Still overloaded, backoff more
        _flowControlTimer->expires_after(std::chrono::milliseconds(50));
        _flowControlTimer->async_wait(
            [this](const boost::system::error_code &ec)
            {
                OnResumeRead(ec);
            }
        );
    }
    else
    {
        // Resuming
        RegisterRecv();
    }
}

void AsioSession::OnError(const std::string &errorMsg)
{
    LOG_ERROR("Session {} Error: {}", _id, errorMsg);
    Close();
}

} // namespace System
