#include "System/Session/Session.h"
#include "System/Debug/MemoryMetrics.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ILog.h"
#include "System/Network/IPacketEncryption.h"
#include "System/Packet/IPacket.h"
#include "System/Packet/PacketHeader.h"
#include "System/Pch.h"
#include "System/Session/SessionFactory.h"
#include "System/Session/SessionPool.h"

#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/recycling_allocator.hpp>

namespace System {

Session::Session()
{
}

Session::~Session()
{
    LOG_INFO("Session Destroyed: ID {}", _id);
}

// Pool reuse
void Session::Reset()
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

void Session::OnRecycle()
{
    if (_socket && _socket->is_open())
        Close();
    _socket.reset();
    _dispatcher = nullptr;
    _encryption.reset(); // Clear encryption on recycle
}

void Session::Reset(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint64_t sessionId, IDispatcher *dispatcher)
{
    _socket = socket;
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
    }

    // Session specific initialization] Pre-allocate timer and buffer
    if (_socket && _socket->is_open())
    {
        _flowControlTimer = std::make_unique<boost::asio::steady_timer>(_socket->get_executor());
    }

    // [Memory Optimization] Pre-allocate linearize buffer
    _linearBuffer.reserve(64 * 1024);
    _recvBuffer.Reset();
    ClearSendQueue();

    // [RateLimiter] Apply config from SessionFactory
    _ingressLimiter.UpdateConfig(SessionFactory::GetRateLimit(), SessionFactory::GetRateBurst());
    _violationCount = 0;

    // [Heartbeat] Init
    _lastRecvTime = std::chrono::steady_clock::now();
    if (_socket && _socket->is_open())
    {
        _heartbeatTimer = std::make_unique<boost::asio::steady_timer>(_socket->get_executor());
    }
}

void Session::GracefulClose()
{
    if (!_gracefulShutdown.exchange(true))
    {
        // Flush will drain remaining packets
    }
}

void Session::OnConnect()
{
    LOG_INFO("Session Connected: ID {}", _id);
    _connected.store(true, std::memory_order_relaxed);

    System::EventMessage *msg = System::MessagePool::AllocateEvent();
    msg->type = (uint32_t)System::MessageType::NETWORK_CONNECT;
    msg->sessionId = _id;
    msg->session = this;
    if (_dispatcher)
    {
        IncRef(); // [Lifetime] Protect session until Dispatcher processes message
        _dispatcher->Post(msg);
    }
    else
        System::MessagePool::Free(msg);

    StartRead();
}

void Session::OnDisconnect()
{
    if (_connected.exchange(false))
    {
        System::EventMessage *msg = System::MessagePool::AllocateEvent();
        msg->type = (uint32_t)System::MessageType::NETWORK_DISCONNECT;
        msg->sessionId = _id;
        msg->session = this;
        if (_dispatcher)
        {
            IncRef(); // [Lifetime] Protect session until Dispatcher processes message
            _dispatcher->Post(msg);
        }
        else
            System::MessagePool::Free(msg);
    }
}

void Session::SendPacket(const IPacket &pkt)
{
    if (!_connected.load(std::memory_order_relaxed))
        return;

    uint16_t size = pkt.GetTotalSize();
    auto msg = MessagePool::AllocatePacket(size);
    if (!msg)
        return;

    pkt.SerializeTo(msg->Payload());

    // [Encryption] Encrypt inside packet payload (In-place)
    if (_encryption)
    {
        _encryption->Encrypt(msg->Payload(), msg->Payload(), size);
    }

    EnqueueSend(msg);
}

void Session::SendPreSerialized(const PacketMessage *source)
{
    if (!_connected.load(std::memory_order_relaxed))
        return;

    // [Single Ownership] Always allocate a NEW message for this session.
    // This avoids shared state, atomic contention, and false sharing.
    auto msg = MessagePool::AllocatePacket(source->length);
    if (!msg)
        return;

    // Copy Content (Header + Body)
    // Serialization was already done once in source.
    std::memcpy(msg->Payload(), source->Payload(), source->length);

    // [Encryption]
    // If enabled, we encrypt strictly IN-PLACE on the *private copy*.
    if (_encryption)
    {
        _encryption->Encrypt(msg->Payload(), msg->Payload(), source->length);
    }

    // Now this session owns 'msg' exclusively.
    EnqueueSend(msg);
}

void Session::Close()
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

// ========== Read Section ==========

void Session::StartRead()
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
                this->DecRef();
            }
        )
    );
}

void Session::OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred)
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

void Session::OnRecv(size_t bytesTransferred)
{

    // Rate Limiter Check
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

        System::PacketHeader *header = reinterpret_cast<System::PacketHeader *>(_recvBuffer.ReadPos());

        if (header->size > 1024 * 10)
        {
            LOG_ERROR("Session {} Packet Too Large: {}", _id, header->size);
            Close();
            return;
        }

        if (dataSize < header->size)
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

        msg->sessionId = _id;
        msg->session = this;

        // [Decryption] Decrypt directly into target buffer (Decrypt-on-Copy)
        if (_encryption)
        {
            _encryption->Decrypt(_recvBuffer.ReadPos(), msg->Payload(), header->size);
        }
        else
        {
            std::memcpy(msg->Payload(), _recvBuffer.ReadPos(), header->size);
        }

        if (_dispatcher)
        {
            IncRef(); // [Lifetime] Protect session until Dispatcher processes message
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
                        this->DecRef();
                    }
                );
            }
            return;
        }
    }

    StartRead();
}

void Session::OnResumeRead(const boost::system::error_code &ec)
{
    if (ec || !_connected.load(std::memory_order_relaxed))
        return;

    // [Safety] Only proceed if we're actually paused
    // Prevents duplicate timer scheduling
    if (!_readPaused.load(std::memory_order_relaxed))
        return;

    if (_dispatcher && !_dispatcher->IsRecovered())
    {
        // Still overloaded, schedule another backoff
        IncRef();
        _flowControlTimer->expires_after(std::chrono::milliseconds(50));
        _flowControlTimer->async_wait(
            [this](const boost::system::error_code &ec)
            {
                OnResumeRead(ec);
                this->DecRef();
            }
        );
    }
    else
    {
        // Recovered, resume reading
        _readPaused.store(false, std::memory_order_relaxed);
        StartRead();
    }
}

// ========== Write Section (Integrated from Writer) ==========

void Session::EnqueueSend(PacketMessage *msg)
{
    _sendQueue.enqueue(msg);

    if (_isSending.exchange(true) == false)
    {
        Flush();
    }
}

void Session::Flush()
{
    if (!_socket || !_socket->is_open())
    {
        _isSending.store(false);
        return;
    }

    static const size_t MAX_BATCH_SIZE = 1000;
    PacketMessage *tempItems[MAX_BATCH_SIZE];

    size_t count = _sendQueue.try_dequeue_bulk(tempItems, MAX_BATCH_SIZE);

    // [Monitoring]
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
                "[Writer] Flush Calls: {}, Avg Batch: {:.2f}, Max Batch: {}", _statFlushCount, avgBatch, _statMaxBatch
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
                            this->DecRef();
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

    // Linearize
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
                this->DecRef();
            }
        )
    );
}

void Session::OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred)
{
    if (ec)
    {
        // [Error Policy A] Clear queue on error - stale packets are discarded
        _isSending.store(false);
        ClearSendQueue();
        return;
    }
    Flush();
}

void Session::ClearSendQueue()
{
    PacketMessage *dummy = nullptr;
    while (_sendQueue.try_dequeue(dummy))
    {
        MessagePool::Free(dummy);
    }
    _isSending.store(false);
}

void Session::OnError(const std::string &errorMsg)
{
    LOG_ERROR("Session {} Error: {}", _id, errorMsg);
    Close();
}

std::string Session::GetRemoteAddress() const
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

void Session::SetEncryption(std::unique_ptr<IPacketEncryption> encryption)
{
    _encryption = std::move(encryption);
}

void Session::ConfigHeartbeat(uint32_t intervalMs, uint32_t timeoutMs, std::function<void(Session *)> pingFunc)
{
    _hbInterval = intervalMs;
    _hbTimeout = timeoutMs;
    _hbPingFunc = pingFunc;

    if (_hbInterval > 0 && _heartbeatTimer)
    {
        StartHeartbeat();
    }
}

void Session::OnPong()
{
    _lastRecvTime = std::chrono::steady_clock::now();
}

void Session::StartHeartbeat()
{
    if (!_heartbeatTimer || _hbInterval == 0)
        return;

    _heartbeatTimer->expires_after(std::chrono::milliseconds(1000)); // Check every second (or interval)
    IncRef();
    _heartbeatTimer->async_wait(
        [this](const boost::system::error_code &ec)
        {
            OnHeartbeatTimer(ec);
            DecRef();
        }
    );
}

void Session::OnHeartbeatTimer(const boost::system::error_code &ec)
{
    if (ec || !_connected.load(std::memory_order_relaxed))
        return;

    auto now = std::chrono::steady_clock::now();

    // 1. Check Timeout
    auto inactiveDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastRecvTime).count();
    if (inactiveDuration > _hbTimeout)
    {
        LOG_WARN("Session {} Heartbeat Timeout ({}ms inactive). Disconnecting.", _id, inactiveDuration);
        Close();
        return;
    }

    // 2. Send Ping
    auto timeSinceLastPing = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastPingTime).count();
    if (timeSinceLastPing >= _hbInterval)
    {
        if (_hbPingFunc)
        {
            _hbPingFunc(this);
            _lastPingTime = now;
        }
    }

    // Reschedule
    StartHeartbeat(); // Keep running
}

} // namespace System
