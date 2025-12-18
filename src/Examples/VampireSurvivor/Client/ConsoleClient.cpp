#include "../Protocol/game.pb.h"
#include "../Server/Core/Protocol.h"
#include <boost/asio.hpp>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;

class Client : public std::enable_shared_from_this<Client>
{
public:
    Client(boost::asio::io_context &io_context) : _socket(io_context), _resolver(io_context)
    {
        _recvBuffer.resize(65536);
    }

    void Connect(const std::string &host, const std::string &port)
    {
        auto endpoints = _resolver.resolve(host, port);
        boost::asio::async_connect(
            _socket,
            endpoints,
            [this, self = shared_from_this()](const boost::system::error_code &ec, tcp::endpoint)
            {
                if (!ec)
                {
                    std::cout << "Connected!" << std::endl;
                    DoRead();

                    // Start Login
                    SendLogin();
                }
                else
                {
                    std::cout << "Connect failed: " << ec.message() << std::endl;
                }
            }
        );
    }

    void SendLogin()
    {
        Protocol::C_Login req;
        req.set_username("test_user");
        req.set_password("password");

        SendPacket(SimpleGame::PacketID::C_LOGIN, req);
    }

    void SendEnterLobby()
    {
        Protocol::C_EnterLobby req;
        SendPacket(SimpleGame::PacketID::C_ENTER_LOBBY, req);
    }

    void SendChat(const std::string &msg)
    {
        Protocol::C_Chat req;
        req.set_msg(msg);
        SendPacket(SimpleGame::PacketID::C_CHAT, req);
    }

    void SendCreateRoom()
    {
        Protocol::C_CreateRoom req;
        req.set_wave_pattern_id(1);
        SendPacket(SimpleGame::PacketID::C_CREATE_ROOM, req);
    }

    void SendLeaveRoom()
    {
        Protocol::C_LeaveRoom req;
        SendPacket(SimpleGame::PacketID::C_LEAVE_ROOM, req);
    }

    template <typename T> void SendPacket(uint16_t id, const T &msg)
    {
        size_t size = msg.ByteSizeLong();
        uint16_t packetSize = (uint16_t)(sizeof(SimpleGame::PacketHeader) + size);

        std::vector<uint8_t> buffer(packetSize);
        SimpleGame::PacketHeader *header = (SimpleGame::PacketHeader *)buffer.data();
        header->size = packetSize;
        header->id = id;

        msg.SerializeToArray(buffer.data() + sizeof(SimpleGame::PacketHeader), (int)size);

        bool writeInProgress = !_sendQueue.empty();
        _sendQueue.push_back(std::move(buffer));

        if (!writeInProgress)
        {
            DoWrite();
        }
    }

private:
    void DoRead()
    {
        _socket.async_read_some(
            boost::asio::buffer(_recvBuffer.data() + _writePos, _recvBuffer.size() - _writePos),
            [this, self = shared_from_this()](boost::system::error_code ec, size_t length)
            {
                if (!ec)
                {
                    _writePos += length;
                    ProcessBuffer();
                    DoRead();
                }
                else
                {
                    _socket.close();
                }
            }
        );
    }

    void ProcessBuffer()
    {
        while (_writePos - _readPos >= sizeof(SimpleGame::PacketHeader))
        {
            SimpleGame::PacketHeader *header = (SimpleGame::PacketHeader *)(_recvBuffer.data() + _readPos);
            if (_writePos - _readPos < header->size)
                break;

            ProcessPacket(header);
            _readPos += header->size;
        }

        if (_readPos == _writePos)
        {
            _readPos = _writePos = 0;
        }
    }

    void ProcessPacket(SimpleGame::PacketHeader *header)
    {
        uint8_t *payload = (uint8_t *)header + sizeof(SimpleGame::PacketHeader);
        size_t payloadSize = header->size - sizeof(SimpleGame::PacketHeader);

        switch (header->id)
        {
        case SimpleGame::PacketID::S_LOGIN:
            std::cout << "[Recv] S_LOGIN" << std::endl;
            // Next: Enter Lobby
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            SendEnterLobby();
            break;

        case SimpleGame::PacketID::S_ENTER_LOBBY:
            std::cout << "[Recv] S_ENTER_LOBBY" << std::endl;
            // Send Chat
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            SendChat("Hello Lobby!");
            // Create Room
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            SendCreateRoom();
            break;

        case SimpleGame::PacketID::S_CREATE_ROOM: {
            Protocol::S_CreateRoom pkt;
            pkt.ParseFromArray(payload, (int)payloadSize);
            std::cout << "[Recv] S_CREATE_ROOM ID: " << pkt.room_id() << std::endl;
            // Chat in Room
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            SendChat("Hello Room!");
            // Leave Room
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            SendLeaveRoom();
            break;
        }

        case SimpleGame::PacketID::S_LEAVE_ROOM:
            std::cout << "[Recv] S_LEAVE_ROOM" << std::endl;
            // Chat in Lobby again
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            SendChat("Back in Lobby!");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            exit(0); // Test Done
            break;

        case SimpleGame::PacketID::S_CHAT: {
            Protocol::S_Chat pkt;
            pkt.ParseFromArray(payload, (int)payloadSize);
            std::cout << "[Recv] Chat: " << pkt.msg() << " (from " << pkt.player_id() << ")" << std::endl;
            break;
        }

        default:
            std::cout << "[Recv] ID: " << header->id << std::endl;
            break;
        }
    }

    void DoWrite()
    {
        boost::asio::async_write(
            _socket,
            boost::asio::buffer(_sendQueue.front()),
            [this, self = shared_from_this()](boost::system::error_code ec, size_t /*length*/)
            {
                if (!ec)
                {
                    _sendQueue.pop_front();
                    if (!_sendQueue.empty())
                    {
                        DoWrite();
                    }
                }
                else
                {
                    _socket.close();
                }
            }
        );
    }

    tcp::socket _socket;
    tcp::resolver _resolver;
    std::vector<uint8_t> _recvBuffer;
    size_t _readPos = 0;
    size_t _writePos = 0;
    std::deque<std::vector<uint8_t>> _sendQueue;
};

int main()
{
    try
    {
        boost::asio::io_context io_context;
        auto client = std::make_shared<Client>(io_context);
        client->Connect("127.0.0.1", "9000"); // Assuming port 9000
        io_context.run();
    } catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
