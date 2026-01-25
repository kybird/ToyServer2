#pragma once

#include "System/Dispatcher/IMessage.h"
#include "System/ISession.h"
#include "System/Network/RateLimiter.h"
#include "System/Network/RecvBuffer.h"

#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>


namespace System {

class IDispatcher;
class PacketBroadcast;

/*
    Backend Session - Zero-copy session for internal server communication.
    [Refactored] PIMPL Removed.
*/
class BackendSession : public ISession
{
    friend class PacketBroadcast;

public:
    BackendSession();
    BackendSession(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher);
    virtual ~BackendSession();

    // Pool Hooks
    void Reset() override;
    void Reset(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher);
    void OnRecycle() override;

    // Graceful Shutdown
    void GracefulClose();

    // Callbacks
    void OnConnect();
    void OnDisconnect();

    // ISession Interface
    void SendPacket(const IPacket &pkt) override;
    void Close() override;
    uint64_t GetId() const override;

    void OnError(const std::string &errorMsg);

    std::thread::id GetDispatcherThreadId() const;
    std::string GetRemoteAddress() const;

    // Heartbeat
    void ConfigHeartbeat(uint32_t intervalMs, uint32_t timeoutMs, std::function<void(BackendSession *)> pingFunc);
    void OnPong();

    // Lifetime safety
    void IncRef() override;
    void DecRef() override;
    bool CanDestroy() const override;
    bool IsConnected() const;

private:
    void SendPreSerialized(const PacketMessage *source);

    // Internal Methods
    void StartRead();
    void OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred);
    void OnRecv(size_t bytesTransferred);
    void OnResumeRead(const boost::system::error_code &ec);

    void EnqueueSend(PacketMessage *msg);
    void Flush();
    void OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred);
    void ClearSendQueue();

    void StartHeartbeat();
    void OnHeartbeatTimer(const boost::system::error_code &ec);

private:
    // Network & State
    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    uint64_t _id = 0;
    IDispatcher *_dispatcher = nullptr;

    // Heartbeat State
    std::unique_ptr<boost::asio::steady_timer> _heartbeatTimer;
    std::chrono::steady_clock::time_point _lastRecvTime;
    std::chrono::steady_clock::time_point _lastPingTime;
    uint32_t _hbInterval = 0;
    uint32_t _hbTimeout = 0;
    std::function<void(BackendSession *)> _hbPingFunc;

    std::thread::id _dispatcherThreadId;

    std::atomic<bool> _connected{false};
    std::atomic<bool> _gracefulShutdown{false};
    std::atomic<int> _ioRef{0};

    // Read State
    RecvBuffer _recvBuffer;
    std::unique_ptr<boost::asio::steady_timer> _flowControlTimer;
    std::atomic<bool> _readPaused{false};

    // Write State
    moodycamel::ConcurrentQueue<PacketMessage *> _sendQueue;
    std::vector<uint8_t> _linearBuffer;
    std::atomic<bool> _isSending{false};

    // Monitoring
    size_t _statFlushCount = 0;
    size_t _statTotalItemCount = 0;
    size_t _statMaxBatch = 0;
    std::chrono::steady_clock::time_point _lastStatTime;

    // Rate Limiting
    RateLimiter _ingressLimiter;
    int _violationCount = 0;
};

} // namespace System
