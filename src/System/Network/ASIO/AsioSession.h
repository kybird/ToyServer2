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

class AsioSession : public ISession, public std::enable_shared_from_this<AsioSession>
{
public:
    AsioSession();
    virtual ~AsioSession();

    void Reset(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint64_t sessionId, IDispatcher *dispatcher);

    // Component Callbacks
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

    // Component Callbacks
    void OnRead(size_t bytesTransferred);
    void OnError(const std::string &errorMsg);

private:
    void RegisterRecv();

private:
    friend class Reader;
    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    uint64_t _id = 0;
    IDispatcher *_dispatcher = nullptr;
    std::atomic<bool> _connected = false;

    // Components
    Reader _reader;
    Writer _writer;

    // Receive State
    RecvBuffer _recvBuffer;

    // Strand for serialization
    // boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    // Keep simple for now, add strand logic in Dispatcher phase
};

} // namespace System
