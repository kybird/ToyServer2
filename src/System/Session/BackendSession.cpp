#include "System/Session/BackendSession.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ILog.h"
#include "System/Network/RecvBuffer.h"
#include "System/Packet/PacketHeader.h"
#include "System/Packet/PacketPtr.h"
#include "System/Pch.h"

#include <boost/asio.hpp>
#include <iostream>

namespace System {

struct BackendSessionImpl
{
    BackendSession *_owner;
    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;

    // Networking Buffers
    RecvBuffer _recvBuffer;

    // Scatter-gather buffers
    std::vector<boost::asio::const_buffer> _gatherBuffers;
    std::vector<PacketPtr> _sendingPackets;

    std::unique_ptr<boost::asio::steady_timer> _flowControlTimer;
    std::unique_ptr<boost::asio::steady_timer> _heartbeatTimer;

    std::chrono::steady_clock::time_point _lastRecvTime;
    std::chrono::steady_clock::time_point _lastPingTime;
    uint32_t _hbInterval = 0;
    uint32_t _hbTimeout = 0;
    std::function<void(BackendSession *)> _hbPingFunc;

    bool _readPaused = false;

    BackendSessionImpl(BackendSession *owner) : _owner(owner)
    {
    }

    void StartRead();
    void OnReadComplete(const boost::system::error_code &ec, size_t tr);
    void OnRecv(size_t tr);

    void StartHeartbeat();
    void OnHeartbeatTimer(const boost::system::error_code &ec);

    void OnWriteComplete(const boost::system::error_code &ec, size_t tr);
};

BackendSession::BackendSession() : _impl(std::make_unique<BackendSessionImpl>(this))
{
}
BackendSession::~BackendSession()
{
}

void BackendSession::Reset()
{
    Session::Reset();
    _impl->_socket.reset();
    _impl->_recvBuffer.Reset();
    _impl->_flowControlTimer.reset();
    _impl->_heartbeatTimer.reset();
    _impl->_gatherBuffers.clear();
    _impl->_sendingPackets.clear();
}

void BackendSession::Reset(const std::shared_ptr<void> &socket, uint64_t sessionId, IDispatcher *dispatcher)
{
    Session::Reset(); // Ensure base state & queue are cleared first

    _impl->_gatherBuffers.clear();
    _impl->_sendingPackets.clear();

    _id = sessionId;
    _dispatcher = dispatcher;

    _impl->_socket = std::static_pointer_cast<boost::asio::ip::tcp::socket>(socket);
    if (_impl->_socket && _impl->_socket->is_open())
    {
        _impl->_flowControlTimer = std::make_unique<boost::asio::steady_timer>(_impl->_socket->get_executor());
        _impl->_heartbeatTimer = std::make_unique<boost::asio::steady_timer>(_impl->_socket->get_executor());
    }
    _impl->_recvBuffer.Reset();
    _impl->_gatherBuffers.reserve(1000);
    _impl->_sendingPackets.reserve(1000);
    _impl->_lastRecvTime = std::chrono::steady_clock::now();
}

void BackendSession::OnRecycle()
{
    if (_impl->_socket && _impl->_socket->is_open())
    {
        Close();
    }

    _impl->_gatherBuffers.clear();
    _impl->_sendingPackets.clear();
    _impl->_socket.reset();

    Session::Reset();
}

void BackendSession::Close()
{
    if (_impl->_socket && _impl->_socket->is_open())
    {
        boost::system::error_code ec;
        _impl->_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        _impl->_socket->close(ec);
    }
    OnDisconnect();
}

void BackendSession::OnConnect()
{
    _connected.store(true);
    System::EventMessage *msg = MessagePool::AllocateEvent();
    msg->type = MessageType::NETWORK_CONNECT;
    msg->sessionId = GetId();
    msg->session = this;
    if (_dispatcher != nullptr)
    {
        IncRef();
        _dispatcher->Post(msg);
    }
    else
        MessagePool::Free(msg);
    _impl->StartRead();
}

void BackendSession::OnDisconnect()
{
    if (_connected.exchange(false))
    {
        System::EventMessage *msg = MessagePool::AllocateEvent();
        msg->type = MessageType::NETWORK_DISCONNECT;
        msg->sessionId = GetId();
        msg->session = this;
        if (_dispatcher != nullptr)
        {
            IncRef();
            _dispatcher->Post(msg);
        }
        else
            MessagePool::Free(msg);
    }
}

void BackendSession::ConfigHeartbeat(
    uint32_t intervalMs, uint32_t timeoutMs, std::function<void(BackendSession *)> pingFunc
)
{
    _impl->_hbInterval = intervalMs;
    _impl->_hbTimeout = timeoutMs;
    _impl->_hbPingFunc = pingFunc;
    if (_impl->_hbInterval > 0 && _impl->_heartbeatTimer)
        _impl->StartHeartbeat();
}

void BackendSession::OnPong()
{
    _impl->_lastRecvTime = std::chrono::steady_clock::now();
}

std::string BackendSession::GetRemoteAddress() const
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

void BackendSession::OnError(const std::string &err)
{
    LOG_ERROR("BackendSession {} Error: {}", _id, err);
    Close();
}

void BackendSession::Flush()
{
    if (!_impl->_socket || !_impl->_socket->is_open())
    {
        _isSending.store(false);
        return;
    }

    static const size_t MAX_BATCH = 1000;
    PacketMessage *tempItems[MAX_BATCH];
    size_t count = _sendQueue.try_dequeue_bulk(tempItems, MAX_BATCH);

    if (count == 0)
    {
        _isSending.store(false);
        PacketMessage *straggler = nullptr;
        if (_sendQueue.try_dequeue(straggler))
        {
            if (!_isSending.exchange(true))
            {
                _sendQueue.enqueue(straggler);
                Flush();
            }
            else
            {
                _sendQueue.enqueue(straggler);
            }
        }
        return;
    }

    _impl->_gatherBuffers.clear();
    _impl->_sendingPackets.clear();

    for (size_t i = 0; i < count; ++i)
    {
        PacketMessage *msg = tempItems[i];
        _impl->_gatherBuffers.push_back(boost::asio::buffer(msg->Payload(), msg->length));
        _impl->_sendingPackets.emplace_back(msg);
    }

    IncRef();
    boost::asio::async_write(
        *_impl->_socket,
        _impl->_gatherBuffers,
        [this](auto ec, auto tr)
        {
            _impl->_gatherBuffers.clear();
            _impl->_sendingPackets.clear();
            _impl->OnWriteComplete(ec, tr);
            DecRef();
        }
    );
}

void BackendSessionImpl::StartRead()
{
    if (!_socket || !_socket->is_open())
        return;
    _recvBuffer.Clean();
    _owner->IncRef();
    _socket->async_read_some(
        boost::asio::buffer(_recvBuffer.WritePos(), _recvBuffer.FreeSize()),
        [this](auto ec, auto tr)
        {
            OnReadComplete(ec, tr);
            _owner->DecRef();
        }
    );
}

void BackendSessionImpl::OnReadComplete(const boost::system::error_code &ec, size_t tr)
{
    if (ec)
        _owner->Close();
    else
        OnRecv(tr);
}

void BackendSessionImpl::OnRecv(size_t tr)
{
    if (!_recvBuffer.MoveWritePos(tr))
    {
        _owner->Close();
        return;
    }
    while (true)
    {
        int32_t ds = _recvBuffer.DataSize();
        if (ds < (int32_t)sizeof(PacketHeader))
            break;
        PacketHeader *h = (PacketHeader *)_recvBuffer.ReadPos();
        if (h->size > 1024 * 10 || h->size == 0)
        {
            _owner->Close();
            return;
        }
        if (ds < h->size)
            break;

        PacketMessage *msg = MessagePool::AllocatePacket(h->size);
        if (!msg)
        {
            _owner->Close();
            return;
        }
        msg->type = MessageType::NETWORK_DATA;
        msg->sessionId = _owner->GetId();
        msg->session = _owner;
        std::memcpy(msg->Payload(), _recvBuffer.ReadPos(), h->size);
        if (_owner->_dispatcher)
        {
            _owner->IncRef();
            _owner->_dispatcher->Post(msg);
        }
        else
            MessagePool::Free(msg);
        _recvBuffer.MoveReadPos(h->size);
    }
    StartRead();
}

void BackendSessionImpl::StartHeartbeat()
{
    _heartbeatTimer->expires_after(std::chrono::milliseconds(1000));
    _owner->IncRef();
    _heartbeatTimer->async_wait(
        [this](auto ec)
        {
            OnHeartbeatTimer(ec);
            _owner->DecRef();
        }
    );
}

void BackendSessionImpl::OnHeartbeatTimer(const boost::system::error_code &ec)
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

void BackendSessionImpl::OnWriteComplete(const boost::system::error_code &ec, size_t)
{
    if (ec)
        _owner->Close();
    else
        _owner->Flush();
}

} // namespace System
