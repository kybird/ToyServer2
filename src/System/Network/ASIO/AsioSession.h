#pragma once

#include "Share/Protocol.h"
#include "System/Network/ASIO/Components/Reader.h"
#include "System/Network/ASIO/Components/Writer.h"
#include "System/Network/ASIO/RecvBuffer.h"
#include "System/Network/ISession.h"
#include "System/Network/Packet.h"
#include "System/Pch.h"
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <memory>
#include <vector>

namespace System {

class IDispatcher;

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
    void Send(boost::intrusive_ptr<Packet> packet) override;
    void Close() override;
    uint64_t GetId() const override
    {
        return _id;
    }

    void OnRead(size_t bytesTransferred);
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
        return _ioRef.load(std::memory_order_acquire) == 0;
    }

private:
    void RegisterRecv();

private:
    friend class Reader;
    friend class Writer;

    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    uint64_t _id = 0;
    IDispatcher *_dispatcher = nullptr;

    std::thread::id _dispatcherThreadId;

    std::atomic<bool> _connected = false;
    std::atomic<bool> _gracefulShutdown = false;
    std::atomic<int> _ioRef = 0;

    Reader _reader;
    Writer _writer;

    RecvBuffer _recvBuffer;

    // [Flow Control]
    std::unique_ptr<boost::asio::steady_timer> _flowControlTimer;
    void OnResumeRead(const boost::system::error_code &ec);
};

} // namespace System
