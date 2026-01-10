#pragma once

#include "System/Dispatcher/IMessage.h"
#include "System/ISession.h"
#include "System/Network/RateLimiter.h" // Added
#include "System/Network/RecvBuffer.h"

#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>

#include <vector>

namespace System {

class IDispatcher;
struct IPacketEncryption; // Forward declaration
class PacketBroadcast;

/*
    High-Performance AsioSession for MMORPG Servers.

    [Design] Reader/Writer integrated directly (no separate classes)
    [Lifetime] Manual RefCounting (IncRef/DecRef) for async operations
    [Hot Path] Minimal atomics, no virtual calls, no shared_ptr in handlers
*/
class Session : public ISession
{
    friend class PacketBroadcast;

public:
    Session();
    Session(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint64_t sessionId, IDispatcher *dispatcher);
    virtual ~Session();

    // Pool Hooks
    void Reset() override;
    void Reset(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint64_t sessionId, IDispatcher *dispatcher);
    void OnRecycle() override;

    // Graceful Shutdown
    void GracefulClose();

    // Callbacks
    void OnConnect();
    void OnDisconnect();

    // ISession Interface
    void SendPacket(const IPacket &pkt) override;
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

    std::string GetRemoteAddress() const; // Added helper

    // Encryption
    void SetEncryption(std::unique_ptr<IPacketEncryption> encryption);

    // Heartbeat (Ping/Pong)
    void ConfigHeartbeat(uint32_t intervalMs, uint32_t timeoutMs, std::function<void(Session *)> pingFunc);
    void OnPong(); // Call this when C_PONG received

    // Lifetime safety
    void IncRef() override
    {
        _ioRef.fetch_add(1, std::memory_order_relaxed);
    }
    void DecRef() override
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
    void OnRecv(size_t bytesTransferred); // Renamed from ProcessReceivedData
    void OnResumeRead(const boost::system::error_code &ec);

    // ========== Write Section ==========
    void EnqueueSend(PacketMessage *msg);
    void Flush();
    void HandleRead(const boost::system::error_code &error, size_t bytesTransferred);
    void HandleWrite(const boost::system::error_code &error, size_t bytesTransferred);
    void OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred);
    void ClearSendQueue();

    // [Phase 3] Broadcast-safe internal sender
    void SendPreSerialized(const PacketMessage *source);

private:
    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    uint64_t _id = 0;
    IDispatcher *_dispatcher = nullptr;
    std::unique_ptr<IPacketEncryption> _encryption; // Added

    // Heartbeat State
    std::unique_ptr<boost::asio::steady_timer> _heartbeatTimer;
    std::chrono::steady_clock::time_point _lastRecvTime;
    std::chrono::steady_clock::time_point _lastPingTime;
    uint32_t _hbInterval = 0;
    uint32_t _hbTimeout = 0;
    std::function<void(Session *)> _hbPingFunc;
    void StartHeartbeat();
    void OnHeartbeatTimer(const boost::system::error_code &ec);

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

    // Rate Limiting
    RateLimiter _ingressLimiter;
    int _violationCount = 0;
};

} // namespace System
