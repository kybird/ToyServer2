#pragma once
#include "Share/Protocol.h"
#include "System/Pch.h"
#include <atomic>
#include <boost/asio.hpp>
#include <deque>
#include <functional>
#include <google/protobuf/message.h>
#include <memory>
#include <vector>

using boost::asio::ip::tcp;

class StressTestClient : public std::enable_shared_from_this<StressTestClient>
{
public:
    using OnRoomCreatedCallback = std::function<void(int roomId)>;

    StressTestClient(boost::asio::io_context &io_context, int id);
    ~StressTestClient();

    void Start(const std::string &host, const std::string &port);
    void Stop();

private:
    void DoConnect(const boost::asio::ip::tcp::resolver::results_type &endpoints, int retriesLeft);

public:
    // Set Target Room ID for Join
    void SetTargetRoom(int roomId)
    {
        _targetRoomId = roomId;
    }

    // Request Room Creation
    void RequestCreateRoom(const std::string &title, OnRoomCreatedCallback callback);

    bool IsConnected() const
    {
        return _isConnected;
    }
    bool IsLoggedIn() const
    {
        return _isLoggedIn;
    }
    bool IsInRoom() const
    {
        return _isInRoom;
    }
    int GetId() const
    {
        return _id;
    }

    void Update(); // Called periodically for Ping/Move

private:
    void SendLogin();
    void SendCreateRoom();
    void SendJoinRoom();
    void SendGameReady();
    void SendPing();
    void SendPong(int64_t timestamp);
    void SendMove();

    void RecvLoop();
    void HandlePacket(uint16_t id, const uint8_t *payload, size_t size);

    void SendPacket(uint16_t id, const google::protobuf::Message &msg);
    void DoWrite();

private:
    tcp::socket _socket;
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    tcp::resolver _resolver;
    std::vector<uint8_t> _recvBuffer;
    std::deque<std::vector<uint8_t>> _sendQueue;

    std::atomic<int> _id;
    std::atomic<int> _targetRoomId = 0;

    std::atomic<bool> _isConnected = false;
    std::atomic<bool> _isLoggedIn = false;
    std::atomic<bool> _isInRoom = false;
    std::atomic<bool> _isReady = false;

    // Room Creation State
    bool _isCreator = false;
    std::string _roomTitleToCreate;
    OnRoomCreatedCallback _onRoomCreated;

    // Stats
    size_t _readPos = 0;
    size_t _writePos = 0;
};
