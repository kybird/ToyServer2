#include "System/Network/ASIO/AsioSession.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/ILog.h"
#include "System/Memory/PacketPool.h"
#include "System/Pch.h"

namespace System {

// Reader implementation moved to System/Network/ASIO/Components/Reader.cpp

AsioSession::AsioSession()
{
}

AsioSession::~AsioSession()
{
}

void AsioSession::Reset(
    std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint64_t sessionId, IDispatcher *dispatcher
)
{
    _socket = socket;
    _id = sessionId;
    _dispatcher = dispatcher;

    if (_socket && _socket->is_open())
    {
        boost::system::error_code ec;

        boost::asio::ip::tcp::no_delay noDelay(true);
        _socket->set_option(noDelay, ec);

        boost::asio::socket_base::receive_buffer_size recvOption(4 * 1024 * 1024);
        _socket->set_option(recvOption, ec);

        boost::asio::socket_base::send_buffer_size sendOption(4 * 1024 * 1024);
        _socket->set_option(sendOption, ec);
    }

    _reader.Init(_socket, this);
    _writer.Init(_socket, this);
    _recvBuffer.Reset();
}

void AsioSession::OnConnect()
{
    LOG_INFO("Session Connected: ID {}", _id);
    _connected.store(true);

    SystemMessage msg;
    msg.type = MessageType::NETWORK_CONNECT;
    msg.sessionId = _id;
    msg.session = shared_from_this(); // Direct Pointer
    if (_dispatcher)
        _dispatcher->Post(msg);

    RegisterRecv();
}

void AsioSession::OnDisconnect()
{
    // LOG_INFO("Session Disconnected: ID {}", _id); // Logged in Dispatcher
    if (_connected.exchange(false))
    {
        SystemMessage msg;
        msg.type = MessageType::NETWORK_DISCONNECT;
        msg.sessionId = _id;
        msg.session = shared_from_this(); // Direct Pointer
        if (_dispatcher)
            _dispatcher->Post(msg);
    }
}

void AsioSession::Send(std::span<const uint8_t> data)
{
    if (!_connected)
        return;
    auto packet = PacketPool::Allocate(data.size());

    packet->assign(data.begin(), data.end());

    _writer.Send(packet);
}

void AsioSession::Send(std::shared_ptr<std::vector<uint8_t>> packet)
{
    if (!_connected)
        return;
    _writer.Send(packet);
}

void AsioSession::Close()
{
    if (_socket && _socket->is_open())
    {
        boost::system::error_code ec;
        _socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        _socket->close(ec);
    }
    // Ensure Disconnect is triggered
    OnDisconnect();
}

void AsioSession::RegisterRecv()
{
    if (_socket->is_open())
    {
        _recvBuffer.Clean(); // Compact if needed
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

    // Process Packets Loop
    while (true)
    {
        int32_t dataSize = _recvBuffer.DataSize();
        if (dataSize < sizeof(Share::PacketHeader))
        {
            break; // Not enough for header
        }

        // Peek Header
        Share::PacketHeader *header = reinterpret_cast<Share::PacketHeader *>(_recvBuffer.ReadPos());

        // Validate Size
        if (header->size > 1024 * 10)
        {
            LOG_ERROR("Session {} Invalid Packet Size: {}", _id, header->size);
            Close();
            return;
        }

        if (dataSize < header->size)
        {
            break; // Not enough for full packet
        }

        // Full Packet Available
        // Dispatch
        SystemMessage msg;
        msg.type = MessageType::NETWORK_DATA;
        msg.sessionId = _id;
        msg.session = shared_from_this(); // Direct Pointer

        // Create a copy for Dispatcher (Safe async ownership)
        // Use PacketPool to reuse vectors
        auto packetData = PacketPool::Allocate(header->size);
        packetData->assign(_recvBuffer.ReadPos(), _recvBuffer.ReadPos() + header->size);

        msg.data = packetData;
        if (_dispatcher)
            _dispatcher->Post(msg);
        // Send(packetData); // Removed immediate echo

        // Consume Packet
        _recvBuffer.OnRead(header->size);
    }

    RegisterRecv();
}

void AsioSession::OnError(const std::string &errorMsg)
{
    LOG_ERROR("Session {} Error: {}", _id, errorMsg);
    Close();
}

} // namespace System
  // namespace System
