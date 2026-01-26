#include "System/Session/BackendSession.h"
#include "System/Debug/MemoryMetrics.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ILog.h"
#include "System/Packet/IPacket.h"
#include "System/Packet/PacketHeader.h"
#include "System/Pch.h"
#include "System/Session/SessionCommon.h"
#include "System/Session/SessionFactory.h"
#include "System/Session/SessionPool.h"

#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/steady_timer.hpp>
#include <concurrentqueue/moodycamel/concurrentqueue.h>

namespace System {

BackendSession::BackendSession()
{
    _lastStatTime = std::chrono::steady_clock::now();
}

BackendSession::BackendSession(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher)
{
    Reset(socket, sessionId, dispatcher);
    _lastStatTime = std::chrono::steady_clock::now();
}

BackendSession::~BackendSession()
{
    LOG_INFO("BackendSession Destroyed: ID {}", GetId());
}

void BackendSession::Reset()
{
    _id = 0;
    _connected.store(false, std::memory_order_relaxed);
    _gracefulShutdown.store(false, std::memory_order_relaxed);
    _ioRef.store(0, std::memory_order_relaxed);
    _readPaused.store(false, std::memory_order_relaxed);
    _isSending.store(false, std::memory_order_relaxed);
    _socket.reset();
    _recvBuffer.Reset();
    _flowControlTimer.reset();
}

void BackendSession::Reset(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher)
{
    _socket = std::static_pointer_cast<boost::asio::ip::tcp::socket>(socket);
    _id = sessionId;
    _dispatcher = dispatcher;

    _connected.store(false, std::memory_order_relaxed);
    _gracefulShutdown.store(false, std::memory_order_relaxed);
    _ioRef.store(0, std::memory_order_relaxed);
    _readPaused.store(false, std::memory_order_relaxed);
    _isSending.store(false, std::memory_order_relaxed);

    if (_socket && _socket->is_open())
    {
        boost::asio::ip::tcp::no_delay noDelay(true);
        boost::system::error_code ec;
        _socket->set_option(noDelay, ec);

        boost::asio::socket_base::receive_buffer_size recvOption(4 * 1024 * 1024);
        _socket->set_option(recvOption, ec);

        boost::asio::socket_base::send_buffer_size sendOption(4 * 1024 * 1024);
        _socket->set_option(sendOption, ec);

        _flowControlTimer = std::make_unique<boost::asio::steady_timer>(_socket->get_executor());
        _heartbeatTimer = std::make_unique<boost::asio::steady_timer>(_socket->get_executor());
    }

    _linearBuffer.reserve(64 * 1024);
    _recvBuffer.Reset();
    ClearSendQueue();

    _ingressLimiter.UpdateConfig(SessionFactory::GetRateLimit(), SessionFactory::GetRateBurst());
    _violationCount = 0;

    _lastRecvTime = std::chrono::steady_clock::now();
}

void BackendSession::OnRecycle()
{
    if (_socket && _socket->is_open())
        Close();
    _socket.reset();
    _dispatcher = nullptr;
}

void BackendSession::Close()
{
    if (_socket && _socket->is_open())
    {
        boost::system::error_code ec;
        _socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        _socket->close(ec);
    }
    ClearSendQueue();
    OnDisconnect();
}

void BackendSession::GracefulClose()
{
    if (!_gracefulShutdown.exchange(true))
    {
        // Pending flush logic handled implicitly
    }
}

void BackendSession::StartRead()
{
    if (!_socket || !_socket->is_open())
        return;

    _recvBuffer.Clean();

    IncRef();

    _socket->async_read_some(
        boost::asio::buffer(_recvBuffer.WritePos(), _recvBuffer.FreeSize()),
        boost::asio::bind_allocator(
            boost::asio::recycling_allocator<void>(),
            [this](const boost::system::error_code &ec, size_t bytesTransferred)
            {
                OnReadComplete(ec, bytesTransferred);
                DecRef();
            }
        )
    );
}

void BackendSession::OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred)
{
    if (ec)
    {
        if (ec != boost::asio::error::eof && ec != boost::asio::error::connection_reset)
        {
            LOG_ERROR("Read Error: {}", ec.message());
        }
        Close();
        return;
    }
    OnRecv(bytesTransferred);
}

// [Backend Hot Path] Zero-copy path (no encryption, direct memcpy)
void BackendSession::OnRecv(size_t bytesTransferred)
{
    if (!_ingressLimiter.TryConsume(1.0))
    {
        LOG_WARN("Session {} Rate Limited! (violation: {})", _id, _violationCount);
        _violationCount++;
        if (_violationCount > 20)
        {
            LOG_ERROR("Session Disconnected due to Rate Limit Violated: {}", GetRemoteAddress());
            Close();
        }
        return;
    }

    if (!_recvBuffer.MoveWritePos(bytesTransferred))
    {
        Close();
        return;
    }

    while (true)
    {
        int32_t dataSize = _recvBuffer.DataSize();
        if (dataSize < static_cast<int32_t>(sizeof(System::PacketHeader)))
            break;

        System::PacketHeader *header = SessionCommon::GetPacketHeader(_recvBuffer.ReadPos());

        if (!SessionCommon::IsValidPacketSize(header->size))
        {
            LOG_ERROR("Session {} Packet Too Large: {}", _id, header->size);
            Close();
            return;
        }

        if (!SessionCommon::HasCompletePacket(dataSize, header->size))
            break;

#ifdef ENABLE_DIAGNOSTICS
        System::Debug::MemoryMetrics::RecvPacket.fetch_add(1, std::memory_order_relaxed);
#endif

        System::PacketMessage *msg = System::MessagePool::AllocatePacket(header->size);
        if (!msg)
        {
#ifdef ENABLE_DIAGNOSTICS
            System::Debug::MemoryMetrics::AllocFail.fetch_add(1, std::memory_order_relaxed);
#endif
            Close();
            return;
        }

        msg->type = System::MessageType::NETWORK_DATA; // Critical Fix
        msg->sessionId = _id;
        msg->session = this;

        // [Backend Path] Plain memcpy (no encryption, no branching)
        std::memcpy(msg->Payload(), _recvBuffer.ReadPos(), header->size);

        if (_dispatcher)
        {
            IncRef();
            _dispatcher->Post(msg);
#ifdef ENABLE_DIAGNOSTICS
            System::Debug::MemoryMetrics::Posted.fetch_add(1, std::memory_order_relaxed);
#endif
        }
        else
            System::MessagePool::Free(msg);

        _recvBuffer.MoveReadPos(header->size);

        if (_dispatcher && _dispatcher->IsOverloaded())
        {
            if (!_readPaused.exchange(true) && _flowControlTimer)
            {
                IncRef();
                _flowControlTimer->expires_after(std::chrono::milliseconds(10));
                _flowControlTimer->async_wait(
                    [this](const boost::system::error_code &ec)
                    {
                        OnResumeRead(ec);
                        DecRef();
                    }
                );
            }
            return;
        }
    }

    StartRead();
}

void BackendSession::OnResumeRead(const boost::system::error_code &ec)
{
    if (ec || !_connected.load(std::memory_order_relaxed))
        return;

    if (!_readPaused.load(std::memory_order_relaxed))
        return;

    if (_dispatcher && !_dispatcher->IsRecovered())
    {
        IncRef();
        _flowControlTimer->expires_after(std::chrono::milliseconds(50));
        _flowControlTimer->async_wait(
            [this](const boost::system::error_code &ec)
            {
                OnResumeRead(ec);
                DecRef();
            }
        );
    }
    else
    {
        _readPaused.store(false, std::memory_order_relaxed);
        StartRead();
    }
}

void BackendSession::EnqueueSend(PacketMessage *msg)
{
    _sendQueue.enqueue(msg);
    if (_isSending.exchange(true) == false)
    {
        Flush();
    }
}

void BackendSession::Flush()
{
    if (!_socket || !_socket->is_open())
    {
        _isSending.store(false);
        return;
    }

    static const size_t MAX_BATCH_SIZE = 1000;
    PacketMessage *tempItems[MAX_BATCH_SIZE];

    size_t count = _sendQueue.try_dequeue_bulk(tempItems, MAX_BATCH_SIZE);

    if (count > 0)
    {
        _statFlushCount++;
        _statTotalItemCount += count;
        if (count > _statMaxBatch)
            _statMaxBatch = count;

        auto now = std::chrono::steady_clock::now();
        if (now - _lastStatTime > std::chrono::seconds(1))
        {
            double avgBatch = (double)_statTotalItemCount / _statFlushCount;
            LOG_FILE(
                "[Backend Writer] Flush Calls: {}, Avg Batch: {:.2f}, Max Batch: {}",
                _statFlushCount,
                avgBatch,
                _statMaxBatch
            );

            _statFlushCount = 0;
            _statTotalItemCount = 0;
            _statMaxBatch = 0;
            _lastStatTime = now;
        }
    }

    if (count == 0)
    {
        _isSending.store(false);

        PacketMessage *straggler = nullptr;
        if (_sendQueue.try_dequeue(straggler))
        {
            bool expected = false;
            if (_isSending.compare_exchange_strong(expected, true))
            {
                _linearBuffer.clear();
                size_t size = straggler->length;
                if (_linearBuffer.capacity() < size)
                    _linearBuffer.reserve(size);
                _linearBuffer.resize(size);

                // [Backend Path] Plain memcpy (no encryption)
                std::memcpy(_linearBuffer.data(), straggler->Payload(), size);
                MessagePool::Free(straggler);

                IncRef();

                boost::asio::async_write(
                    *_socket,
                    boost::asio::buffer(_linearBuffer),
                    boost::asio::bind_allocator(
                        boost::asio::recycling_allocator<void>(),
                        [this](const boost::system::error_code &ec, size_t tr)
                        {
                            OnWriteComplete(ec, tr);
                            DecRef();
                        }
                    )
                );
                return;
            }
            else
            {
                _sendQueue.enqueue(straggler);
            }
        }
        return;
    }

    // Linearize without encryption
    size_t totalSize = 0;
    for (size_t i = 0; i < count; ++i)
    {
        totalSize += tempItems[i]->length;
    }

    _linearBuffer.clear();
    if (_linearBuffer.capacity() < totalSize)
    {
        _linearBuffer.reserve(std::max(totalSize, _linearBuffer.capacity() * 2));
    }
    _linearBuffer.resize(totalSize);

    uint8_t *destPtr = _linearBuffer.data();
    for (size_t i = 0; i < count; ++i)
    {
        size_t pktSize = tempItems[i]->length;

        // [Backend Path] Plain memcpy (no encryption, no branching)
        std::memcpy(destPtr, tempItems[i]->Payload(), pktSize);

        destPtr += pktSize;
        MessagePool::Free(tempItems[i]);
    }

    IncRef();

    boost::asio::async_write(
        *_socket,
        boost::asio::buffer(_linearBuffer),
        boost::asio::bind_allocator(
            boost::asio::recycling_allocator<void>(),
            [this](const boost::system::error_code &ec, size_t bytesTransferred)
            {
                OnWriteComplete(ec, bytesTransferred);
                DecRef();
            }
        )
    );
}

void BackendSession::OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred)
{
    if (ec)
    {
        _isSending.store(false);
        ClearSendQueue();
        return;
    }
    Flush();
}

void BackendSession::ClearSendQueue()
{
    PacketMessage *dummy = nullptr;
    while (_sendQueue.try_dequeue(dummy))
    {
        MessagePool::Free(dummy);
    }
    _isSending.store(false);
}

std::string BackendSession::GetRemoteAddress() const
{
    if (!_socket || !_socket->is_open())
        return "Unknown";
    try
    {
        return _socket->remote_endpoint().address().to_string();
    } catch (...)
    {
        return "Unknown";
    }
}

void BackendSession::OnHeartbeatTimer(const boost::system::error_code &ec)
{
    if (ec || !_connected.load(std::memory_order_relaxed))
        return;

    auto now = std::chrono::steady_clock::now();
    auto inactiveDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastRecvTime).count();
    if (inactiveDuration > _hbTimeout)
    {
        LOG_WARN("Session {} Heartbeat Timeout ({}ms).", _id, inactiveDuration);
        Close();
        return;
    }

    auto timeSinceLastPing = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastPingTime).count();
    if (timeSinceLastPing >= _hbInterval)
    {
        if (_hbPingFunc)
        {
            _hbPingFunc(this);
            _lastPingTime = now;
        }
    }

    StartHeartbeat();
}

void BackendSession::StartHeartbeat()
{
    if (!_heartbeatTimer || _hbInterval == 0)
        return;

    _heartbeatTimer->expires_after(std::chrono::milliseconds(1000));
    IncRef();
    _heartbeatTimer->async_wait(
        [this](const boost::system::error_code &ec)
        {
            OnHeartbeatTimer(ec);
            DecRef();
        }
    );
}

void BackendSession::OnConnect()
{
    LOG_INFO("BackendSession Connected: ID {}", GetId());
    _connected.store(true, std::memory_order_relaxed);

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

    StartRead();
}

void BackendSession::OnDisconnect()
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

void BackendSession::SendPacket(const IPacket &pkt)
{
    if (!_connected.load(std::memory_order_relaxed))
        return;

    uint16_t size = pkt.GetTotalSize();
    auto msg = MessagePool::AllocatePacket(size);
    if (!msg)
        return;

    pkt.SerializeTo(msg->Payload());

    EnqueueSend(msg);
}

void BackendSession::SendPreSerialized(const PacketMessage *source)
{
    if (!_connected.load(std::memory_order_relaxed))
        return;

    source->AddRef();
    EnqueueSend(const_cast<PacketMessage *>(source));
}

uint64_t BackendSession::GetId() const
{
    return _id;
}

void BackendSession::OnError(const std::string &errorMsg)
{
    LOG_ERROR("BackendSession {} Error: {}", GetId(), errorMsg);
    Close();
}

std::thread::id BackendSession::GetDispatcherThreadId() const
{
    return _dispatcherThreadId;
}

void BackendSession::ConfigHeartbeat(
    uint32_t intervalMs, uint32_t timeoutMs, std::function<void(BackendSession *)> pingFunc
)
{
    _hbInterval = intervalMs;
    _hbTimeout = timeoutMs;
    _hbPingFunc = pingFunc;

    if (_hbInterval > 0 && _heartbeatTimer)
    {
        StartHeartbeat();
    }
}

void BackendSession::OnPong()
{
    _lastRecvTime = std::chrono::steady_clock::now();
}

void BackendSession::IncRef()
{
    _ioRef.fetch_add(1, std::memory_order_relaxed);
}

void BackendSession::DecRef()
{
    _ioRef.fetch_sub(1, std::memory_order_release);
}

bool BackendSession::CanDestroy() const
{
    return !_connected.load(std::memory_order_relaxed) && _ioRef.load(std::memory_order_acquire) == 0;
}

bool BackendSession::IsConnected() const
{
    return _connected.load(std::memory_order_relaxed);
}

} // namespace System
