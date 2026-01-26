#include "System/Session/GatewaySession.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ILog.h"
#include "System/Network/IPacketEncryption.h"
#include "System/Network/RecvBuffer.h"
#include "System/Packet/PacketHeader.h"
#include "System/Pch.h"

#include <boost/asio.hpp>
#include <iostream>

namespace System {

struct GatewaySessionImpl
{
    GatewaySession *_owner;
    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    std::unique_ptr<IPacketEncryption> _encryption;

    // Networking Buffers
    RecvBuffer _recvBuffer;
    std::vector<uint8_t> _linearBuffer;

    // Timers
    std::unique_ptr<boost::asio::steady_timer> _flowControlTimer;
    std::unique_ptr<boost::asio::steady_timer> _heartbeatTimer;

    // Heartbeat State
    std::chrono::steady_clock::time_point _lastRecvTime;
    std::chrono::steady_clock::time_point _lastPingTime;
    uint32_t _hbInterval = 0;
    uint32_t _hbTimeout = 0;
    std::function<void(GatewaySession *)> _hbPingFunc;

    bool _readPaused = false;

    GatewaySessionImpl(GatewaySession *owner) : _owner(owner)
    {
    }

    void StartRead();
    void OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred);
    void OnRecv(size_t bytesTransferred);
    void OnResumeRead(const boost::system::error_code &ec);

    void StartHeartbeat();
    void OnHeartbeatTimer(const boost::system::error_code &ec);

    void OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred);
};

GatewaySession::GatewaySession() : _impl(std::make_unique<GatewaySessionImpl>(this))
{
}

GatewaySession::~GatewaySession()
{
}

void GatewaySession::Reset()
{
    Session::Reset();
    _impl->_socket.reset();
    _impl->_encryption.reset();
    _impl->_recvBuffer.Reset();
    _impl->_readPaused = false;
    _impl->_flowControlTimer.reset();
    _impl->_heartbeatTimer.reset();
}

void GatewaySession::Reset(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher)
{
    Session::Reset(); // Ensure base state & queue are cleared first

    _id = sessionId;
    _dispatcher = dispatcher;

    _impl->_socket = std::static_pointer_cast<boost::asio::ip::tcp::socket>(socket);

    if (_impl->_socket && _impl->_socket->is_open())
    {
        boost::asio::ip::tcp::no_delay noDelay(true);
        boost::system::error_code ec;
        _impl->_socket->set_option(noDelay, ec);

        _impl->_flowControlTimer = std::make_unique<boost::asio::steady_timer>(_impl->_socket->get_executor());
        _impl->_heartbeatTimer = std::make_unique<boost::asio::steady_timer>(_impl->_socket->get_executor());
    }

    _impl->_recvBuffer.Reset();
    _impl->_linearBuffer.reserve(64 * 1024);
    _impl->_lastRecvTime = std::chrono::steady_clock::now();
}

void GatewaySession::OnRecycle()
{
    if (_impl->_socket && _impl->_socket->is_open())
        Close();
    _impl->_socket.reset();
    _impl->_encryption.reset();
}

void GatewaySession::Close()
{
    if (_impl->_socket && _impl->_socket->is_open())
    {
        boost::system::error_code ec;
        _impl->_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        _impl->_socket->close(ec);
    }
    OnDisconnect();
}

void GatewaySession::OnConnect()
{
    LOG_INFO("GatewaySession Connected: ID {}", GetId());
    _connected.store(true);

    System::EventMessage *msg = System::MessagePool::AllocateEvent();
    msg->type = System::MessageType::NETWORK_CONNECT;
    msg->sessionId = GetId();
    msg->session = this;
    if (_dispatcher)
    {
        IncRef();
        _dispatcher->Post(msg);
    }
    else
        System::MessagePool::Free(msg);

    _impl->StartRead();
}

void GatewaySession::OnDisconnect()
{
    if (_connected.exchange(false))
    {
        System::EventMessage *msg = System::MessagePool::AllocateEvent();
        msg->type = System::MessageType::NETWORK_DISCONNECT;
        msg->sessionId = GetId();
        msg->session = this;
        if (_dispatcher)
        {
            IncRef();
            _dispatcher->Post(msg);
        }
        else
            System::MessagePool::Free(msg);
    }
}

void GatewaySession::SetEncryption(std::unique_ptr<IPacketEncryption> encryption)
{
    _impl->_encryption = std::move(encryption);
}

void GatewaySession::ConfigHeartbeat(
    uint32_t intervalMs, uint32_t timeoutMs, std::function<void(GatewaySession *)> pingFunc
)
{
    _impl->_hbInterval = intervalMs;
    _impl->_hbTimeout = timeoutMs;
    _impl->_hbPingFunc = pingFunc;

    if (_impl->_hbInterval > 0 && _impl->_heartbeatTimer)
    {
        _impl->StartHeartbeat();
    }
}

void GatewaySession::OnPong()
{
    _impl->_lastRecvTime = std::chrono::steady_clock::now();
}

std::string GatewaySession::GetRemoteAddress() const
{
    if (!_impl->_socket || !_impl->_socket->is_open())
        return "Unknown";
    try
    {
        return _impl->_socket->remote_endpoint().address().to_string();
    } catch (...)
    {
        return "Unknown";
    }
}

void GatewaySession::OnError(const std::string &errorMsg)
{
    LOG_ERROR("GatewaySession {} Error: {}", GetId(), errorMsg);
    Close();
}

void GatewaySession::Flush()
{
    if (!_impl->_socket || !_impl->_socket->is_open())
    {
        _isSending.store(false);
        return;
    }

    static const size_t MAX_BATCH_SIZE = 1000;
    PacketMessage *tempItems[MAX_BATCH_SIZE];

    size_t count = _sendQueue.try_dequeue_bulk(tempItems, MAX_BATCH_SIZE);
    if (count == 0)
    {
        _isSending.store(false);

        // Re-check for new items that might have arrived just after dequeue
        PacketMessage *straggler = nullptr;
        if (_sendQueue.try_dequeue(straggler))
        {
            if (!_isSending.exchange(true))
            {
                _sendQueue.enqueue(straggler);
                Flush(); // Recurse to process straggler
            }
            else
            {
                _sendQueue.enqueue(straggler);
            }
        }
        return;
    }

    // Linearize with Encryption
    size_t totalSize = 0;
    for (size_t i = 0; i < count; ++i)
        totalSize += tempItems[i]->length;

    _impl->_linearBuffer.clear();
    _impl->_linearBuffer.resize(totalSize);

    uint8_t *destPtr = _impl->_linearBuffer.data();
    for (size_t i = 0; i < count; ++i)
    {
        size_t pktSize = tempItems[i]->length;
        if (_impl->_encryption)
        {
            // Copy Header (Plain)
            std::memcpy(destPtr, tempItems[i]->Payload(), sizeof(PacketHeader));
            // Encrypt Payload
            if (pktSize > sizeof(PacketHeader))
            {
                _impl->_encryption->Encrypt(
                    tempItems[i]->Payload() + sizeof(PacketHeader),
                    destPtr + sizeof(PacketHeader),
                    pktSize - sizeof(PacketHeader)
                );
            }
        }
        else
        {
            std::memcpy(destPtr, tempItems[i]->Payload(), pktSize);
        }
        destPtr += pktSize;
        MessagePool::Free(tempItems[i]);
    }

    IncRef();
    boost::asio::async_write(
        *_impl->_socket,
        boost::asio::buffer(_impl->_linearBuffer),
        [this](const boost::system::error_code &ec, size_t bytesTransferred)
        {
            _impl->OnWriteComplete(ec, bytesTransferred);
            DecRef();
        }
    );
}

// ----------------------------------------------------------------------------
// GatewaySessionImpl
// ----------------------------------------------------------------------------

void GatewaySessionImpl::StartRead()
{
    if (!_socket || !_socket->is_open())
        return;

    _recvBuffer.Clean();
    _owner->IncRef();

    _socket->async_read_some(
        boost::asio::buffer(_recvBuffer.WritePos(), _recvBuffer.FreeSize()),
        [this](const boost::system::error_code &ec, size_t tr)
        {
            OnReadComplete(ec, tr);
            _owner->DecRef();
        }
    );
}

void GatewaySessionImpl::OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred)
{
    if (ec)
    {
        _owner->Close();
        return;
    }
    OnRecv(bytesTransferred);
}

void GatewaySessionImpl::OnRecv(size_t bytesTransferred)
{
    if (!_recvBuffer.MoveWritePos(bytesTransferred))
    {
        _owner->Close();
        return;
    }

    while (true)
    {
        int32_t dataSize = _recvBuffer.DataSize();
        if (dataSize < (int32_t)sizeof(PacketHeader))
            break;

        PacketHeader *header = (PacketHeader *)_recvBuffer.ReadPos();
        if (header->size > 1024 * 10 || header->size == 0)
        {
            _owner->Close();
            return;
        }

        if (dataSize < header->size)
            break;

        PacketMessage *msg = MessagePool::AllocatePacket(header->size);
        if (!msg)
        {
            _owner->Close();
            return;
        }

        msg->type = MessageType::NETWORK_DATA;
        msg->sessionId = _owner->GetId();
        msg->session = _owner;

        if (_encryption)
        {
            std::memcpy(msg->Payload(), _recvBuffer.ReadPos(), sizeof(PacketHeader));
            if (header->size > sizeof(PacketHeader))
            {
                _encryption->Decrypt(
                    _recvBuffer.ReadPos() + sizeof(PacketHeader),
                    msg->Payload() + sizeof(PacketHeader),
                    header->size - sizeof(PacketHeader)
                );
            }
        }
        else
        {
            std::memcpy(msg->Payload(), _recvBuffer.ReadPos(), header->size);
        }

        if (_owner->_dispatcher)
        {
            _owner->IncRef();
            _owner->_dispatcher->Post(msg);
        }
        else
            MessagePool::Free(msg);

        _recvBuffer.MoveReadPos(header->size);
    }

    StartRead();
}

void GatewaySessionImpl::StartHeartbeat()
{
    if (!_heartbeatTimer)
        return;
    _heartbeatTimer->expires_after(std::chrono::milliseconds(1000));
    _owner->IncRef();
    _heartbeatTimer->async_wait(
        [this](const boost::system::error_code &ec)
        {
            OnHeartbeatTimer(ec);
            _owner->DecRef();
        }
    );
}

void GatewaySessionImpl::OnHeartbeatTimer(const boost::system::error_code &ec)
{
    if (ec || !_owner->IsConnected())
        return;

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastRecvTime).count() > _hbTimeout)
    {
        _owner->Close();
        return;
    }

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastPingTime).count() >= _hbInterval)
    {
        if (_hbPingFunc)
        {
            _hbPingFunc(_owner);
            _lastPingTime = now;
        }
    }

    StartHeartbeat();
}

void GatewaySessionImpl::OnWriteComplete(const boost::system::error_code &ec, size_t)
{
    if (ec)
    {
        _owner->Close();
        return;
    }
    _owner->Flush(); // Call base virtual Flush
}

} // namespace System
