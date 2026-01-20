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
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/steady_timer.hpp>
#include <concurrentqueue/moodycamel/concurrentqueue.h>

namespace System {

// =========================================================================================
// [PIMPL] SessionImpl Definition
// Holds all Boost Asio dependencies and implementation details.
// =========================================================================================
struct SessionImpl
{
    Session *_owner; // Back-pointer for IncRef/DecRef/GetId

    // Network & State
    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    uint64_t _id = 0;
    IDispatcher *_dispatcher = nullptr;
    std::unique_ptr<IPacketEncryption> _encryption;

    // Heartbeat State
    std::unique_ptr<boost::asio::steady_timer> _heartbeatTimer;
    std::chrono::steady_clock::time_point _lastRecvTime;
    std::chrono::steady_clock::time_point _lastPingTime;
    uint32_t _hbInterval = 0;
    uint32_t _hbTimeout = 0;
    std::function<void(Session *)> _hbPingFunc;

    std::thread::id _dispatcherThreadId;

    std::atomic<bool> _connected = false;
    std::atomic<bool> _gracefulShutdown = false;
    std::atomic<int> _ioRef = 0; // Lifetime ref count

    // Read State
    RecvBuffer _recvBuffer;
    std::unique_ptr<boost::asio::steady_timer> _flowControlTimer;
    std::atomic<bool> _readPaused = false;

    // Write State
    moodycamel::ConcurrentQueue<PacketMessage *> _sendQueue;
    std::vector<uint8_t> _linearBuffer;
    std::atomic<bool> _isSending = false;

    // Monitoring
    size_t _statFlushCount = 0;
    size_t _statTotalItemCount = 0;
    size_t _statMaxBatch = 0;
    std::chrono::steady_clock::time_point _lastStatTime;

    // Rate Limiting
    RateLimiter _ingressLimiter;
    int _violationCount = 0;

    SessionImpl(Session *owner) : _owner(owner)
    {
        _lastStatTime = std::chrono::steady_clock::now();
    }

    // ============================================================================
    // Logic Methods (Moved from Session)
    // ============================================================================

    void Reset()
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

    void OnRecycle()
    {
        if (_socket && _socket->is_open())
            Close();
        _socket.reset();
        _dispatcher = nullptr;
        _encryption.reset();
    }

    void Reset(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint64_t sessionId, IDispatcher *dispatcher)
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

    void Close()
    {
        if (_socket && _socket->is_open())
        {
            boost::system::error_code ec;
            _socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            _socket->close(ec);
        }
        ClearSendQueue();
        _owner->OnDisconnect(); // Call owner's OnDisconnect logic
    }

    void GracefulClose()
    {
        if (!_gracefulShutdown.exchange(true))
        {
            // Pending flush logic handled implicitly?
        }
    }

    void StartRead()
    {
        if (!_socket || !_socket->is_open())
            return;

        _recvBuffer.Clean();

        _owner->IncRef(); // Owner Lifetime

        _socket->async_read_some(
            boost::asio::buffer(_recvBuffer.WritePos(), _recvBuffer.FreeSize()),
            boost::asio::bind_allocator(
                boost::asio::recycling_allocator<void>(),
                [this](const boost::system::error_code &ec, size_t bytesTransferred)
                {
                    OnReadComplete(ec, bytesTransferred);
                    _owner->DecRef();
                }
            )
        );
    }

    void OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred)
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

    void OnRecv(size_t bytesTransferred)
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

            // [Thread Safety Logic]
            // RecvBuffer는 IO 스레드가 독점하며, 로직 스레드(Dispatcher)로는
            // 아래와 같이 새로 할당된 메시지 객체에 데이터를 복사하여 전달합니다.
            // 이를 통해 버퍼 경합 없이 Lock-Free한 구조를 유지합니다.
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
            msg->session = _owner;

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
                _owner->IncRef();
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
                    _owner->IncRef();
                    _flowControlTimer->expires_after(std::chrono::milliseconds(10));
                    _flowControlTimer->async_wait(
                        [this](const boost::system::error_code &ec)
                        {
                            OnResumeRead(ec);
                            _owner->DecRef();
                        }
                    );
                }
                return;
            }
        }

        StartRead();
    }

    void OnResumeRead(const boost::system::error_code &ec)
    {
        if (ec || !_connected.load(std::memory_order_relaxed))
            return;

        if (!_readPaused.load(std::memory_order_relaxed))
            return;

        if (_dispatcher && !_dispatcher->IsRecovered())
        {
            _owner->IncRef();
            _flowControlTimer->expires_after(std::chrono::milliseconds(50));
            _flowControlTimer->async_wait(
                [this](const boost::system::error_code &ec)
                {
                    OnResumeRead(ec);
                    _owner->DecRef();
                }
            );
        }
        else
        {
            _readPaused.store(false, std::memory_order_relaxed);
            StartRead();
        }
    }

    // Write Logic
    void EnqueueSend(PacketMessage *msg)
    {
        _sendQueue.enqueue(msg);
        if (_isSending.exchange(true) == false)
        {
            Flush();
        }
    }

    void Flush()
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
                    "[Writer] Flush Calls: {}, Avg Batch: {:.2f}, Max Batch: {}",
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

                    if (_encryption)
                    {
                        _encryption->Encrypt(straggler->Payload(), _linearBuffer.data(), size);
                    }
                    else
                    {
                        std::memcpy(_linearBuffer.data(), straggler->Payload(), size);
                    }
                    MessagePool::Free(straggler);

                    _owner->IncRef();

                    boost::asio::async_write(
                        *_socket,
                        boost::asio::buffer(_linearBuffer),
                        boost::asio::bind_allocator(
                            boost::asio::recycling_allocator<void>(),
                            [this](const boost::system::error_code &ec, size_t tr)
                            {
                                OnWriteComplete(ec, tr);
                                _owner->DecRef();
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
            if (_encryption)
            {
                // [Optimization] Cryptographic Zero-Copy:
                // 복사와 암호화를 동시에 수행하여 메모리 통과 횟수 감소
                _encryption->Encrypt(tempItems[i]->Payload(), destPtr, pktSize);
            }
            else
            {
                std::memcpy(destPtr, tempItems[i]->Payload(), pktSize);
            }
            destPtr += pktSize;
            MessagePool::Free(tempItems[i]);
        }

        _owner->IncRef();

        boost::asio::async_write(
            *_socket,
            boost::asio::buffer(_linearBuffer),
            boost::asio::bind_allocator(
                boost::asio::recycling_allocator<void>(),
                [this](const boost::system::error_code &ec, size_t bytesTransferred)
                {
                    OnWriteComplete(ec, bytesTransferred);
                    _owner->DecRef();
                }
            )
        );
    }

    void OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if (ec)
        {
            _isSending.store(false);
            ClearSendQueue();
            return;
        }
        Flush();
    }

    void ClearSendQueue()
    {
        PacketMessage *dummy = nullptr;
        while (_sendQueue.try_dequeue(dummy))
        {
            MessagePool::Free(dummy);
        }
        _isSending.store(false);
    }

    std::string GetRemoteAddress() const
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

    void OnHeartbeatTimer(const boost::system::error_code &ec)
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
                _hbPingFunc(_owner);
                _lastPingTime = now;
            }
        }

        StartHeartbeat();
    }

    void StartHeartbeat()
    {
        if (!_heartbeatTimer || _hbInterval == 0)
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
};

// =========================================================================================
// Session Delegation Implementation
// =========================================================================================

Session::Session() : _impl(std::make_unique<SessionImpl>(this))
{
}

Session::Session(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher)
    : _impl(std::make_unique<SessionImpl>(this))
{
    _impl->Reset(std::static_pointer_cast<boost::asio::ip::tcp::socket>(socket), sessionId, dispatcher);
}

Session::~Session()
{
    LOG_INFO("Session Destroyed: ID {}", GetId());
}

void Session::Reset()
{
    _impl->Reset();
}

void Session::Reset(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher)
{
    _impl->Reset(std::static_pointer_cast<boost::asio::ip::tcp::socket>(socket), sessionId, dispatcher);
}

void Session::OnRecycle()
{
    _impl->OnRecycle();
}

void Session::GracefulClose()
{
    _impl->GracefulClose();
}

void Session::OnConnect()
{
    LOG_INFO("Session Connected: ID {}", GetId());
    _impl->_connected.store(true, std::memory_order_relaxed);

    System::EventMessage *msg = System::MessagePool::AllocateEvent();
    msg->type = (uint32_t)System::MessageType::NETWORK_CONNECT;
    msg->sessionId = GetId();
    msg->session = this;
    if (_impl->_dispatcher)
    {
        IncRef();
        _impl->_dispatcher->Post(msg);
    }
    else
        System::MessagePool::Free(msg);

    _impl->StartRead();
}

void Session::OnDisconnect()
{
    if (_impl->_connected.exchange(false))
    {
        System::EventMessage *msg = System::MessagePool::AllocateEvent();
        msg->type = (uint32_t)System::MessageType::NETWORK_DISCONNECT;
        msg->sessionId = GetId();
        msg->session = this;
        if (_impl->_dispatcher)
        {
            IncRef();
            _impl->_dispatcher->Post(msg);
        }
        else
            System::MessagePool::Free(msg);
    }
}

void Session::SendPacket(const IPacket &pkt)
{
    if (!_impl->_connected.load(std::memory_order_relaxed))
        return;

    uint16_t size = pkt.GetTotalSize();
    auto msg = MessagePool::AllocatePacket(size);
    if (!msg)
        return;

    pkt.SerializeTo(msg->Payload());

    // [Note] Encryption is deferred until SessionImpl::Flush
    _impl->EnqueueSend(msg);
}

void Session::SendPreSerialized(const PacketMessage *source)
{
    if (!_impl->_connected.load(std::memory_order_relaxed))
        return;

    // [Optimization] Reference Counting Broadcast:
    // 패킷의 참조 카운트만 늘려서 큐에 삽입. 복사 및 즉각 암호화 제거.
    source->AddRef();
    _impl->EnqueueSend(const_cast<PacketMessage *>(source));
}

void Session::Close()
{
    _impl->Close();
}

uint64_t Session::GetId() const
{
    return _impl->_id;
}

void Session::OnError(const std::string &errorMsg)
{
    LOG_ERROR("Session {} Error: {}", GetId(), errorMsg);
    Close();
}

std::thread::id Session::GetDispatcherThreadId() const
{
    return _impl->_dispatcherThreadId;
}

std::string Session::GetRemoteAddress() const
{
    return _impl->GetRemoteAddress();
}

void Session::SetEncryption(std::unique_ptr<IPacketEncryption> encryption)
{
    _impl->_encryption = std::move(encryption);
}

void Session::ConfigHeartbeat(uint32_t intervalMs, uint32_t timeoutMs, std::function<void(Session *)> pingFunc)
{
    _impl->_hbInterval = intervalMs;
    _impl->_hbTimeout = timeoutMs;
    _impl->_hbPingFunc = pingFunc;

    if (_impl->_hbInterval > 0 && _impl->_heartbeatTimer)
    {
        _impl->StartHeartbeat();
    }
}

void Session::OnPong()
{
    _impl->_lastRecvTime = std::chrono::steady_clock::now();
}

void Session::IncRef()
{
    _impl->_ioRef.fetch_add(1, std::memory_order_relaxed);
}

void Session::DecRef()
{
    _impl->_ioRef.fetch_sub(1, std::memory_order_release);
}

bool Session::CanDestroy() const
{
    return !_impl->_connected.load(std::memory_order_relaxed) && _impl->_ioRef.load(std::memory_order_acquire) == 0;
}

bool Session::IsConnected() const
{
    return _impl->_connected.load(std::memory_order_relaxed);
}

} // namespace System
