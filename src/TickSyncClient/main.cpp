#include "Protocol/game.pb.h"
#include "System/Packet/PacketHeader.h"
#include "System/Pch.h"
#include "System/Utility/Encoding.h"
#include <boost/asio.hpp>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;
using namespace std::chrono;

// 패킷 ID (game.proto의 MsgId 값과 일치해야 함)
namespace PacketID {
constexpr uint16_t C_LOGIN = 100;
constexpr uint16_t S_LOGIN = 101;
constexpr uint16_t C_JOIN_ROOM = 104;
constexpr uint16_t S_JOIN_ROOM = 105;
constexpr uint16_t C_GAME_READY = 114;
constexpr uint16_t S_SPAWN_OBJECT = 200;
constexpr uint16_t S_MOVE_OBJECT_BATCH = 202;
constexpr uint16_t C_PING = 902;
constexpr uint16_t S_PONG = 903;
constexpr uint16_t S_DEBUG_SERVER_TICK = 904;
} // namespace PacketID

// 패킷 헤더 구조 (서버와 동일)
#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t size; // 헤더 포함 전체 크기
    uint16_t id;   // 패킷 ID
};
#pragma pack(pop)
static constexpr size_t HEADER_SIZE = sizeof(PacketHeader);

// 틱 동기화 상태
struct TickSyncState
{
    uint32_t initialServerTick = 0;
    float tickInterval = 0.04f; // 기본값 25 TPS
    uint32_t tickRate = 25;
    steady_clock::time_point tickStartTime;
    bool synced = false;

    // RTT 측정
    int64_t lastPingTimestamp = 0;
    float rttMs = 0.0f;
};

TickSyncState g_SyncState;

// 현재 클라이언트 예측 틱 계산
uint32_t GetCurrentClientTick()
{
    if (!g_SyncState.synced)
        return 0;

    auto elapsed = steady_clock::now() - g_SyncState.tickStartTime;
    // 마이크로초 단위로 계산하여 정밀도 향상
    int64_t elapsedUs = duration_cast<microseconds>(elapsed).count();
    float elapsedSeconds = elapsedUs / 1000000.0f;
    uint32_t ticksPassed = static_cast<uint32_t>(elapsedSeconds / g_SyncState.tickInterval);
    return g_SyncState.initialServerTick + ticksPassed;
}

// 패킷 전송 헬퍼
template <typename ProtoMsg> void SendPacket(tcp::socket &socket, uint16_t packetId, const ProtoMsg &msg)
{
    std::string payload;
    msg.SerializeToString(&payload);

    uint16_t totalSize = static_cast<uint16_t>(HEADER_SIZE + payload.size());
    std::vector<uint8_t> buffer(totalSize);

    PacketHeader *header = reinterpret_cast<PacketHeader *>(buffer.data());
    header->size = totalSize;
    header->id = packetId;

    std::memcpy(buffer.data() + HEADER_SIZE, payload.data(), payload.size());

    boost::asio::write(socket, boost::asio::buffer(buffer));
}

// 패킷 핸들러
void HandlePacket(
    tcp::socket &socket, uint16_t packetId, const uint8_t *payload, size_t payloadSize, bool &loggedIn,
    bool &joinedRoom, bool &sentGameReady
)
{
    switch (packetId)
    {
    case PacketID::S_LOGIN: {
        Protocol::S_Login msg;
        if (msg.ParseFromArray(payload, payloadSize))
        {
            if (msg.success())
            {
                g_SyncState.initialServerTick = msg.server_tick();
                g_SyncState.tickInterval = msg.server_tick_interval();
                g_SyncState.tickRate = msg.server_tick_rate();
                g_SyncState.tickStartTime = steady_clock::now();
                g_SyncState.synced = true;

                std::cout << "[S_LOGIN] Success! Initial ServerTick=" << g_SyncState.initialServerTick
                          << " TickRate=" << g_SyncState.tickRate << " TickInterval=" << g_SyncState.tickInterval << "s"
                          << std::endl;

                loggedIn = true;

                // 로그인 성공 직후 방 입장 시도
                Protocol::C_JoinRoom joinRoom;
                joinRoom.set_room_id(1);
                SendPacket(socket, PacketID::C_JOIN_ROOM, joinRoom);
                std::cout << "[SENT] C_JoinRoom (room_id=1)" << std::endl;
            }
        }
        break;
    }

    case PacketID::S_JOIN_ROOM: {
        Protocol::S_JoinRoom msg;
        if (msg.ParseFromArray(payload, payloadSize))
        {
            std::cout << "[S_JOIN_ROOM] Success=" << msg.success() << " RoomId=" << msg.room_id() << std::endl;

            if (msg.success())
            {
                joinedRoom = true;
                // 방 입장 성공 시 GameReady 전송
                Protocol::C_GameReady gameReady;
                SendPacket(socket, PacketID::C_GAME_READY, gameReady);
                std::cout << "[SENT] C_GameReady" << std::endl;
                sentGameReady = true;
            }
        }
        break;
    }

    case PacketID::S_DEBUG_SERVER_TICK: {
        Protocol::S_DebugServerTick msg;
        if (msg.ParseFromArray(payload, payloadSize))
        {
            uint32_t serverTick = msg.server_tick();
            uint32_t clientTick = GetCurrentClientTick();
            int32_t diff = static_cast<int32_t>(clientTick) - static_cast<int32_t>(serverTick);
            float diffMs = diff * g_SyncState.tickInterval * 1000.0f;

            std::cout << "[TICK SYNC] Server=" << serverTick << " Client=" << clientTick << " Diff=" << diff
                      << " ticks (" << std::fixed << std::setprecision(1) << diffMs << " ms)"
                      << " RTT=" << std::fixed << std::setprecision(1) << g_SyncState.rttMs << " ms" << std::endl;
        }
        break;
    }

    case PacketID::S_MOVE_OBJECT_BATCH: {
        Protocol::S_MoveObjectBatch msg;
        if (msg.ParseFromArray(payload, payloadSize))
        {
            uint32_t serverTick = msg.server_tick();
            uint32_t clientTick = GetCurrentClientTick();
            int32_t diff = static_cast<int32_t>(clientTick) - static_cast<int32_t>(serverTick);

            static int moveCount = 0;
            if (++moveCount % 30 == 0)
            {
                std::cout << "[MOVE BATCH] ServerTick=" << serverTick << " ClientTick=" << clientTick
                          << " Diff=" << diff << std::endl;
            }
        }
        break;
    }

    case PacketID::S_PONG: {
        Protocol::S_Pong msg;
        if (msg.ParseFromArray(payload, payloadSize))
        {
            auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
            g_SyncState.rttMs = static_cast<float>(now - msg.timestamp());
            // std::cout << "[S_PONG] RTT=" << g_SyncState.rttMs << " ms" << std::endl;
        }
        break;
    }

    case PacketID::S_SPAWN_OBJECT: {
        Protocol::S_SpawnObject msg;
        if (msg.ParseFromArray(payload, payloadSize))
        {
            std::cout << "[S_SPAWN_OBJECT] Count=" << msg.objects_size() << std::endl;
        }
        break;
    }

    default:
        break;
    }
}

int main()
{
    try
    {
        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);

        std::cout << "========================================" << std::endl;
        std::cout << " TickSyncClient (C++ Tick Sync Tester)" << std::endl;
        std::cout << "========================================" << std::endl;

        std::cout << "Connecting to 127.0.0.1:9000..." << std::endl;
        auto endpoints = resolver.resolve("127.0.0.1", "9000");
        boost::asio::connect(socket, endpoints);
        std::cout << "Connected!" << std::endl;

        socket.set_option(tcp::no_delay(true));

        // 1. C_Login 전송
        {
            Protocol::C_Login login;
            login.set_username("TickSyncTest_CPP");
            login.set_password("test123");
            SendPacket(socket, PacketID::C_LOGIN, login);
            std::cout << "[SENT] C_Login" << std::endl;
        }

        std::vector<uint8_t> recvBuffer(65536);
        size_t readPos = 0;
        size_t writePos = 0;

        bool loggedIn = false;
        bool joinedRoom = false;
        bool sentGameReady = false;

        auto lastPingTime = steady_clock::now();
        constexpr int PING_INTERVAL_SEC = 5;

        while (true)
        {
            socket.non_blocking(true);
            boost::system::error_code ec;

            size_t bytesReceived =
                socket.read_some(boost::asio::buffer(recvBuffer.data() + writePos, recvBuffer.size() - writePos), ec);

            if (ec != boost::asio::error::would_block)
            {
                if (ec)
                {
                    std::cerr << "Recv error: " << System::Utility::ToUtf8(ec.message()) << std::endl;
                    break;
                }
                writePos += bytesReceived;
            }

            while (writePos - readPos >= HEADER_SIZE)
            {
                PacketHeader *header = reinterpret_cast<PacketHeader *>(recvBuffer.data() + readPos);

                if (header->size > 10240)
                {
                    std::cerr << "Invalid packet size: " << header->size << std::endl;
                    return 1;
                }

                if (writePos - readPos < header->size)
                    break;

                const uint8_t *payload = recvBuffer.data() + readPos + HEADER_SIZE;
                size_t payloadSize = header->size - HEADER_SIZE;
                HandlePacket(socket, header->id, payload, payloadSize, loggedIn, joinedRoom, sentGameReady);

                readPos += header->size;
            }

            if (readPos == writePos)
            {
                readPos = 0;
                writePos = 0;
            }
            else if (readPos > recvBuffer.size() / 2)
            {
                size_t remaining = writePos - readPos;
                std::memmove(recvBuffer.data(), recvBuffer.data() + readPos, remaining);
                readPos = 0;
                writePos = remaining;
            }

            // Ping (RTT measurement)
            auto now = steady_clock::now();
            if (sentGameReady && duration_cast<seconds>(now - lastPingTime).count() >= PING_INTERVAL_SEC)
            {
                lastPingTime = now;
                Protocol::C_Ping ping;
                int64_t timestamp = duration_cast<milliseconds>(now.time_since_epoch()).count();
                ping.set_timestamp(timestamp);
                g_SyncState.lastPingTimestamp = timestamp;
                SendPacket(socket, PacketID::C_PING, ping);
            }

            // std::this_thread::sleep_for(milliseconds(1)); // Removed for accurate RTT measurement
            // If CPU usage is too high, we can use Sleep(0) or yield
            // std::this_thread::yield();
        }

    } catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
