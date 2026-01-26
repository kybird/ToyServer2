#include "System/Session/GatewaySession.h"
#include "System/Debug/MemoryMetrics.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ILog.h"
#include "System/Network/IPacketEncryption.h"
#include "System/Packet/IPacket.h"
#include "System/Packet/PacketHeader.h"
#include "System/Pch.h"
#include "System/Session/SessionCommon.h"
#include "System/Session/SessionFactory.h"
#include "System/Session/SessionPool.h"

#include <iostream>

namespace System {

GatewaySession::GatewaySession()
{
    _lastStatTime = std::chrono::steady_clock::now();
}

GatewaySession::GatewaySession(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher)
{
    Reset(socket, sessionId, dispatcher);
    _lastStatTime = std::chrono::steady_clock::now();
}

GatewaySession::~GatewaySession()
{
    LOG_INFO("GatewaySession Destroyed: ID {}", GetId());
}

void GatewaySession::Reset()
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

void GatewaySession::Reset(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher)
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

void GatewaySession::OnRecycle()
{
    if (_socket && _socket->is_open())
        Close();
    _socket.reset();
    _dispatcher = nullptr;
    _encryption.reset();
}

void GatewaySession::Close()
{
    std::cout << "[DEBUG] GatewaySession::Close() Called for ID " << _id
              << ". SocketOpen: " << (_socket && _socket->is_open()) << std::endl;
    if (_socket && _socket->is_open())
    {
        boost::system::error_code ec;
        _socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        _socket->close(ec);
    }
    ClearSendQueue();
    OnDisconnect();
}

void GatewaySession::GracefulClose()
{
    if (!_gracefulShutdown.exchange(true))
    {
        // Pending flush logic handled implicitly
    }
}

void GatewaySession::StartRead()
{
    if (!_socket || !_socket->is_open())
    {
        std::cerr << "[DEBUG] StartRead Aborted: Socket Closed or Null" << std::endl;
        return;
    }

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

void GatewaySession::OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred)
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

// [Gateway Hot Path] Encryption-only path (no branching)
void GatewaySession::OnRecv(size_t bytesTransferred)
{
    // [DEBUG]

    if (!_ingressLimiter.TryConsume(1.0))
    {
        // Rate Limit temporarily disabled for Debugging
        // LOG_WARN("Session {} Rate Limited! (violation: {})", _id, _violationCount);
        // return;
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

        if (header->size > 1024 * 10 || header->size == 0)
        {
            LOG_ERROR("Session {} Packet Size Error: {}", _id, header->size);
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

        msg->type = System::MessageType::NETWORK_DATA;
        msg->sessionId = _id;
        msg->session = this;

        // [Gateway Path] Decrypt only Payload (Header is plain text)
        if (_encryption)
        {
            // 1. Copy Header as-is (4 bytes)
            std::memcpy(msg->Payload(), _recvBuffer.ReadPos(), sizeof(System::PacketHeader));

            // 2. Decrypt Payload (Remaining bytes)
            if (header->size > sizeof(System::PacketHeader))
            {
                _encryption->Decrypt(
                    _recvBuffer.ReadPos() + sizeof(System::PacketHeader),
                    msg->Payload() + sizeof(System::PacketHeader),
                    header->size - sizeof(System::PacketHeader)
                );
            }
        }
        else
        {
            LOG_ERROR("GatewaySession {} has no encryption configured!", _id);
            MessagePool::Free(msg);
            Close();
            return;
        }

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

void GatewaySession::OnResumeRead(const boost::system::error_code &ec)
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

void GatewaySession::EnqueueSend(PacketMessage *msg)
{
    _sendQueue.enqueue(msg);
    if (_isSending.exchange(true) == false)
    {
        Flush();
    }
}

void GatewaySession::Flush()
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
                "[Gateway Writer] Flush Calls: {}, Avg Batch: {:.2f}, Max Batch: {}",
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

                // [Gateway Path] Encrypt only Payload (Header is plain text)
                if (_encryption)
                {
                    // 1. Copy Header as-is (4 bytes)
                    std::memcpy(_linearBuffer.data(), straggler->Payload(), sizeof(System::PacketHeader));

                    // 2. Encrypt Payload (Remaining bytes)
                    if (size > sizeof(System::PacketHeader))
                    {
                        _encryption->Encrypt(
                            straggler->Payload() + sizeof(System::PacketHeader),
                            _linearBuffer.data() + sizeof(System::PacketHeader),
                            size - sizeof(System::PacketHeader)
                        );
                    }
                }
                else
                {
                    LOG_ERROR("GatewaySession {} has no encryption configured!", _id);
                    MessagePool::Free(straggler);
                    _isSending.store(false);
                    return;
                }
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

    // Linearize with encryption
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

        // [Gateway Path] Encrypt only Payload (Header is plain text)
        if (_encryption)
        {
            // 1. Copy Header as-is (4 bytes)
            std::memcpy(destPtr, tempItems[i]->Payload(), sizeof(System::PacketHeader));

            // 2. Encrypt Payload (Remaining bytes)
            if (pktSize > sizeof(System::PacketHeader))
            {
                _encryption->Encrypt(
                    tempItems[i]->Payload() + sizeof(System::PacketHeader),
                    destPtr + sizeof(System::PacketHeader),
                    pktSize - sizeof(System::PacketHeader)
                );
            }
        }
        else
        {
            LOG_ERROR("GatewaySession {} has no encryption configured!", _id);
            for (size_t j = i; j < count; ++j)
                MessagePool::Free(tempItems[j]);
            _isSending.store(false);
            return;
        }

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

void GatewaySession::OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred)
{
    if (ec)
    {
        _isSending.store(false);
        ClearSendQueue();
        return;
    }
    Flush();
}

void GatewaySession::ClearSendQueue()
{
    PacketMessage *dummy = nullptr;
    while (_sendQueue.try_dequeue(dummy))
    {
        MessagePool::Free(dummy);
    }
    _isSending.store(false);
}

std::string GatewaySession::GetRemoteAddress() const
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

void GatewaySession::OnHeartbeatTimer(const boost::system::error_code &ec)
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

void GatewaySession::StartHeartbeat()
{
    if (!_heartbeatTimer || _hbInterval == 0)
        return;

    _heartbeatTimer->expires_after(std::chrono::milliseconds(1000));
    IncRef();
    // LOG_INFO("[Session] Handshake Started. ID: {}", _id); // Removed spam
    Flush();
    _heartbeatTimer->async_wait(
        [this](const boost::system::error_code &ec)
        {
            OnHeartbeatTimer(ec);
            DecRef();
        }
    );
}

void GatewaySession::OnConnect()
{
    LOG_INFO("GatewaySession Connected: ID {}", GetId());
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

void GatewaySession::SendPacket(const IPacket &pkt)
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

uint64_t GatewaySession::GetId() const
{
    return _id;
}

void GatewaySession::OnError(const std::string &errorMsg)
{
    LOG_ERROR("GatewaySession {} Error: {}", GetId(), errorMsg);
    Close();
}

std::thread::id GatewaySession::GetDispatcherThreadId() const
{
    return _dispatcherThreadId;
}

void GatewaySession::SetEncryption(std::unique_ptr<IPacketEncryption> encryption)
{
    _encryption = std::move(encryption);
}

void GatewaySession::ConfigHeartbeat(
    uint32_t intervalMs, uint32_t timeoutMs, std::function<void(GatewaySession *)> pingFunc
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

void GatewaySession::OnPong()
{
    _lastRecvTime = std::chrono::steady_clock::now();
}

void GatewaySession::IncRef()
{
    _ioRef.fetch_add(1, std::memory_order_relaxed);
}

void GatewaySession::DecRef()
{
    _ioRef.fetch_sub(1, std::memory_order_release);
}

bool GatewaySession::CanDestroy() const
{
    return !_connected.load(std::memory_order_relaxed) && _ioRef.load(std::memory_order_acquire) == 0;
}

bool GatewaySession::IsConnected() const
{
    return _connected.load(std::memory_order_relaxed);
}

} // namespace System
