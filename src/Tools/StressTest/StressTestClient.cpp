#include "StressTestClient.h"
#include "../../Examples/VampireSurvivor/Protocol/game.pb.h"
#include "System/Packet/PacketHeader.h"
#include <iostream>

using namespace Share;

namespace PacketID {
constexpr uint16_t C_LOGIN = 100;
constexpr uint16_t S_LOGIN = 101;
constexpr uint16_t C_CREATE_ROOM = 102;
constexpr uint16_t S_CREATE_ROOM = 103;
constexpr uint16_t C_JOIN_ROOM = 104;
constexpr uint16_t S_JOIN_ROOM = 105;
constexpr uint16_t C_GAME_READY = 114;
constexpr uint16_t C_MOVE_INPUT = 203;
constexpr uint16_t S_PING = 900;
constexpr uint16_t C_PONG = 901;
constexpr uint16_t C_PING = 902;
constexpr uint16_t S_PONG = 903;
} // namespace PacketID

StressTestClient::StressTestClient(boost::asio::io_context &io_context, int id)
    : _socket(io_context), _resolver(io_context), _id(id)
{
    _recvBuffer.resize(8192);
}

StressTestClient::~StressTestClient()
{
    Stop();
}

void StressTestClient::Start(const std::string &host, const std::string &port)
{
    auto endpoints = _resolver.resolve(host, port);
    DoConnect(endpoints, 5); // Start with 5 retries
}

void StressTestClient::DoConnect(const boost::asio::ip::tcp::resolver::results_type &endpoints, int retriesLeft)
{
    boost::asio::async_connect(
        _socket,
        endpoints,
        [this, self = shared_from_this(), endpoints, retriesLeft](const boost::system::error_code &ec, tcp::endpoint)
        {
            if (!ec)
            {
                _isConnected = true;
                _socket.set_option(tcp::no_delay(true));
                // std::cout << "[Client " << _id << "] Connected\n";
                RecvLoop();
                SendLogin();
            }
            else
            {
                if (retriesLeft > 0)
                {
                    // Retry after 1 second
                    auto timer = std::make_shared<boost::asio::steady_timer>(_socket.get_executor());
                    timer->expires_after(std::chrono::seconds(1));
                    timer->async_wait(
                        [this, self, endpoints, retriesLeft, timer](const boost::system::error_code &)
                        {
                            DoConnect(endpoints, retriesLeft - 1);
                        }
                    );
                }
                else
                {
                    std::cerr << "[Client " << _id << "] Connect Failed (Final): " << ec.message() << "\n";
                }
            }
        }
    );
}

void StressTestClient::Stop()
{
    _isConnected = false;
    boost::system::error_code ec;
    try
    {
        if (_socket.is_open())
            _socket.close(ec);
    } catch (...)
    {
    }
}

void StressTestClient::RequestCreateRoom(const std::string &title, OnRoomCreatedCallback callback)
{
    _isCreator = true;
    _roomTitleToCreate = title;
    _onRoomCreated = callback;
}

void StressTestClient::SendPacket(uint16_t id, const google::protobuf::Message &msg)
{
    if (!_isConnected)
        return;

    size_t bodySize = msg.ByteSizeLong();
    size_t packetSize = sizeof(System::PacketHeader) + bodySize;

    std::vector<uint8_t> buffer(packetSize);
    auto *header = reinterpret_cast<System::PacketHeader *>(buffer.data());
    header->size = static_cast<uint16_t>(packetSize);
    header->id = id;

    msg.SerializeToArray(buffer.data() + sizeof(System::PacketHeader), static_cast<int>(bodySize));

    // XOR-CBC Encryption (Key: 165)
    // Server Init: XorEncryption(165)
    // Algorithm: cipher = plain ^ key; key = cipher;
    uint8_t key = 165;
    uint8_t *payloadPtr = buffer.data() + sizeof(System::PacketHeader);
    for (size_t i = 0; i < bodySize; ++i)
    {
        uint8_t plain = payloadPtr[i];
        uint8_t cipher = plain ^ key;
        payloadPtr[i] = cipher;
        key = cipher; // Feedback for CBC
    }

    boost::asio::async_write(
        _socket,
        boost::asio::buffer(buffer),
        [self = shared_from_this()](const boost::system::error_code &, size_t)
        {
        }
    );
}

void StressTestClient::SendLogin()
{
    Protocol::C_Login pkt;
    pkt.set_username("Stress_" + std::to_string(_id));
    pkt.set_password("pass");
    SendPacket(PacketID::C_LOGIN, pkt);
}

void StressTestClient::SendCreateRoom()
{
    Protocol::C_CreateRoom pkt;
    pkt.set_room_title(_roomTitleToCreate);
    pkt.set_wave_pattern_id(1);
    SendPacket(PacketID::C_CREATE_ROOM, pkt);
}

void StressTestClient::SendJoinRoom()
{
    if (_targetRoomId == 0)
        return;

    Protocol::C_JoinRoom pkt;
    pkt.set_room_id(_targetRoomId);
    SendPacket(PacketID::C_JOIN_ROOM, pkt);
}

void StressTestClient::SendGameReady()
{
    Protocol::C_GameReady pkt;
    SendPacket(PacketID::C_GAME_READY, pkt);
}

void StressTestClient::SendPing()
{
    Protocol::C_Ping pkt;
    auto now = std::chrono::steady_clock::now();
    pkt.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    SendPacket(PacketID::C_PING, pkt);
}

void StressTestClient::SendPong(int64_t timestamp)
{
    Protocol::C_Pong pkt;
    pkt.set_timestamp(timestamp);
    SendPacket(PacketID::C_PONG, pkt);
}

void StressTestClient::SendMove()
{
    Protocol::C_MoveInput pkt;
    pkt.set_client_tick(0);
    pkt.set_dir_x(1);
    pkt.set_dir_y(0);
    SendPacket(PacketID::C_MOVE_INPUT, pkt);
}

void StressTestClient::Update()
{
    if (!_isConnected)
        return;

    if (_isInRoom && _isReady)
    {
        // [Real Strategy] Every client should ping occasionally if not moving,
        // but even moving clients should ping to be safe.
        // Send Ping every 10 seconds (main loop is 1s)
        static thread_local int counter = 0;
        if (++counter % 10 == 0)
        {
            SendPing();
        }

        // Send Move input periodically to simulate activity
        SendMove();
    }
}

void StressTestClient::RecvLoop()
{
    _socket.async_read_some(
        boost::asio::buffer(_recvBuffer.data() + _writePos, _recvBuffer.size() - _writePos),
        [this, self = shared_from_this()](const boost::system::error_code &ec, size_t bytesTransferred)
        {
            if (ec)
            {
                // std::cerr << "[Client " << _id << "] Recv Error: " << ec.message() << "\n";
                _isConnected = false;
                return;
            }

            _writePos += bytesTransferred;

            while (_writePos - _readPos >= sizeof(System::PacketHeader))
            {
                auto *header = reinterpret_cast<System::PacketHeader *>(&_recvBuffer[_readPos]);

                if (_writePos - _readPos < header->size)
                    break;

                // Handle Packet
                HandlePacket(
                    header->id,
                    &_recvBuffer[_readPos + sizeof(System::PacketHeader)],
                    header->size - sizeof(System::PacketHeader)
                );

                _readPos += header->size;
            }

            if (_readPos == _writePos)
            {
                _readPos = 0;
                _writePos = 0;
            }
            else if (_recvBuffer.size() - _writePos < 1024)
            {
                // Compact
                size_t remaining = _writePos - _readPos;
                std::memmove(_recvBuffer.data(), &_recvBuffer[_readPos], remaining);
                _readPos = 0;
                _writePos = remaining;
            }

            RecvLoop();
        }
    );
}

void StressTestClient::HandlePacket(uint16_t id, const uint8_t *payload, size_t size)
{
    // XOR-CBC Decryption (Key: 165)
    // Server uses XOR Key 165 for body.
    uint8_t key = 165;
    std::vector<uint8_t> decryptedBuffer(size);
    for (size_t i = 0; i < size; ++i)
    {
        uint8_t cipher = payload[i];
        uint8_t plain = cipher ^ key;
        decryptedBuffer[i] = plain;
        key = cipher; // Feedback for CBC
    }
    const uint8_t *finalPayload = decryptedBuffer.data();

    // std::cout << "[Client " << _id << "] Recv Packet ID: " << id << "\n";
    switch (id)
    {
    case PacketID::S_LOGIN: {
        Protocol::S_Login pkt;
        if (pkt.ParseFromArray(finalPayload, static_cast<int>(size)) && pkt.success())
        {
            _isLoggedIn = true;
            if (_isCreator)
                SendCreateRoom();
            else
                SendJoinRoom();
        }
        else
        {
            std::cerr << "[Client " << _id << "] Login Failed\n";
        }
    }
    break;
    case PacketID::S_CREATE_ROOM: {
        Protocol::S_CreateRoom pkt;
        if (pkt.ParseFromArray(finalPayload, static_cast<int>(size)) && pkt.success())
        {
            // Room created successfully
            int roomId = pkt.room_id();
            // std::cout << "[Client " << _id << "] Room Created: " << roomId << "\n";
            if (_onRoomCreated)
                _onRoomCreated(roomId);

            _targetRoomId = roomId;
            SendJoinRoom();
        }
        else
        {
            std::cerr << "[Client " << _id << "] Create Room Failed\n";
        }
    }
    break;
    case PacketID::S_JOIN_ROOM: {
        Protocol::S_JoinRoom pkt;
        if (pkt.ParseFromArray(finalPayload, static_cast<int>(size)) && pkt.success())
        {
            _isInRoom = true;
            SendGameReady();
            _isReady = true;
        }
    }
    break;
    case PacketID::S_PING: {
        Protocol::S_Ping pkt;
        if (pkt.ParseFromArray(finalPayload, static_cast<int>(size)))
        {
            // [Critical] Anti-AFK / Heartbeat Response
            SendPong(pkt.timestamp());
        }
    }
    break;
    case PacketID::S_PONG: {
        // Latency check could be here
    }
    break;
    default:
        break;
    }
}
