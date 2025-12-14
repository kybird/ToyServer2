#pragma once

#include "Share/Protocol.h"
#include "System/Dispatcher/IMessage.h"
#include "System/Network/ASIO/RecvBuffer.h"
#include "System/Network/ISession.h"
#include "System/Pch.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>
#include <vector>

namespace System {

class IDispatcher;

/*
    High-Performance AsioSession for MMORPG Servers.

    [Design] Reader/Writer integrated directly (no separate classes)
    [Lifetime] Manual RefCounting (IncRef/DecRef) for async operations
    [Hot Path] Minimal atomics, no virtual calls, no shared_ptr in handlers
*/
class AsioSession : public ISession
{
public:
    AsioSession();
    virtual ~AsioSession();

    // Pool Hooks
    void Reset() override;
    void OnRecycle() override;

    // Initialization for new connection
    void Reset(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint64_t sessionId, IDispatcher *dispatcher);

    // Graceful Shutdown
    void GracefulClose();

    // Callbacks
    void OnConnect();
    void OnDisconnect();

    // ISession Interface
    void Send(std::span<const uint8_t> data) override;
    void Send(PacketMessage *msg) override;
    void Close() override;
    uint64_t GetId() const override
    {
        return _id;
    }

    void OnError(const std::string &errorMsg);

    std::thread::id GetDispatcherThreadId() const
    {
        return _dispatcherThreadId;
    }

    // Lifetime safety
    void IncRef()
    {
        _ioRef.fetch_add(1, std::memory_order_relaxed);
    }
    void DecRef()
    {
        _ioRef.fetch_sub(1, std::memory_order_release);
    }
    bool CanDestroy() const override
    {
        // [Safety] Both conditions required:
        // 1. Not connected (socket closed)
        // 2. No pending IO/message references
        return !_connected.load(std::memory_order_relaxed) && _ioRef.load(std::memory_order_acquire) == 0;
    }

    // [Direct Access] Check if session is connected (no vtable)
    bool IsConnected() const
    {
        return _connected.load(std::memory_order_relaxed);
    }

private:
    // ========== Read Section ==========
    void StartRead();
    void OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred);
    void ProcessReceivedData(size_t bytesTransferred);
    void OnResumeRead(const boost::system::error_code &ec);

    // ========== Write Section ==========
    void EnqueueSend(PacketMessage *msg);
    void Flush();
    void OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred);
    void ClearSendQueue();

private:
    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    uint64_t _id = 0;
    IDispatcher *_dispatcher = nullptr;

    std::thread::id _dispatcherThreadId;

    std::atomic<bool> _connected = false;
    std::atomic<bool> _gracefulShutdown = false;
    std::atomic<int> _ioRef = 0;

    // ========== Read State ==========
    RecvBuffer _recvBuffer;
    std::unique_ptr<boost::asio::steady_timer> _flowControlTimer;
    std::atomic<bool> _readPaused = false;

    // ========== Write State (Integrated from Writer) ==========
    moodycamel::ConcurrentQueue<PacketMessage *> _sendQueue;
    std::vector<uint8_t> _linearBuffer;
    std::atomic<bool> _isSending = false;

    // [Monitoring]
    size_t _statFlushCount = 0;
    size_t _statTotalItemCount = 0;
    size_t _statMaxBatch = 0;
    std::chrono::steady_clock::time_point _lastStatTime = std::chrono::steady_clock::now();
};

} // namespace System
