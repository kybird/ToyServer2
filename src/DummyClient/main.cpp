#include "Share/Protocol.h"
#include "System/Pch.h"
#include <atomic>
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;

std::atomic<int> g_ConnectedCount = 0;
std::atomic<long long> g_SendCount = 0;
std::atomic<long long> g_RecvCount = 0;
std::atomic<bool> g_IsRunning = true;

// [Latency Tracking]
std::atomic<long long> g_TotalLatencyUs = 0; // 총 레이턴시 (마이크로초)
std::atomic<long long> g_LatencySamples = 0; // 샘플 수
std::atomic<long long> g_MinLatencyUs = LLONG_MAX;
std::atomic<long long> g_MaxLatencyUs = 0;

// 원자적 최소값 갱신
void UpdateMinLatency(long long value)
{
    long long prev = g_MinLatencyUs.load();
    while (value < prev && !g_MinLatencyUs.compare_exchange_weak(prev, value))
        ;
}

// 원자적 최대값 갱신
void UpdateMaxLatency(long long value)
{
    long long prev = g_MaxLatencyUs.load();
    while (value > prev && !g_MaxLatencyUs.compare_exchange_weak(prev, value))
        ;
}

class ClientSession : public std::enable_shared_from_this<ClientSession>
{
public:
    ClientSession(boost::asio::io_context &io_context) : _socket(io_context), _resolver(io_context)
    {
        _recvBuffer.resize(65536);
    }

    void Start(const std::string &host, const std::string &port)
    {
        auto endpoints = _resolver.resolve(host, port);
        boost::asio::async_connect(
            _socket,
            endpoints,
            [this, self = shared_from_this()](const boost::system::error_code &ec, tcp::endpoint)
            {
                if (!ec)
                {
                    g_ConnectedCount++;

                    boost::asio::socket_base::receive_buffer_size option(1024 * 1024);
                    _socket.set_option(option);

                    tcp::no_delay noDelay(true);
                    _socket.set_option(noDelay);

                    SendLoop();
                    RecvLoop();
                }
            }
        );
    }

    void StopSending()
    {
        _sendStopped = true;
    }

    void Close()
    {
        boost::system::error_code ec;
        if (_socket.is_open())
        {
            _socket.shutdown(tcp::socket::shutdown_both, ec);
            _socket.close(ec);
        }
    }

private:
    void RecvLoop()
    {
        _socket.async_read_some(
            boost::asio::buffer(_recvBuffer.data() + _writePos, _recvBuffer.size() - _writePos),
            [this, self = shared_from_this()](const boost::system::error_code &ec, size_t bytesTransferred)
            {
                if (ec)
                    return;

                _writePos += bytesTransferred;

                while (_writePos - _readPos >= sizeof(Share::PacketHeader))
                {
                    Share::PacketHeader *header = reinterpret_cast<Share::PacketHeader *>(&_recvBuffer[_readPos]);

                    if (header->size > 1024 * 10)
                    {
                        return;
                    }

                    if (_writePos - _readPos < header->size)
                    {
                        break;
                    }

                    // =========================================================
                    // [Latency Measurement] Extract timestamp from packet
                    // =========================================================
                    size_t payloadSize = header->size - sizeof(Share::PacketHeader);
                    if (payloadSize >= sizeof(int64_t))
                    {
                        const uint8_t *payload = &_recvBuffer[_readPos + sizeof(Share::PacketHeader)];
                        int64_t sendTime;
                        std::memcpy(&sendTime, payload, sizeof(int64_t));

                        auto now = std::chrono::steady_clock::now();
                        int64_t nowUs =
                            std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

                        int64_t rtt = nowUs - sendTime;
                        if (rtt > 0 && rtt < 10000000) // 유효 범위: 0 ~ 10초
                        {
                            g_TotalLatencyUs.fetch_add(rtt, std::memory_order_relaxed);
                            g_LatencySamples.fetch_add(1, std::memory_order_relaxed);
                            UpdateMinLatency(rtt);
                            UpdateMaxLatency(rtt);
                        }
                    }
                    // =========================================================

                    g_RecvCount++;
                    _readPos += header->size;
                }

                if (_readPos == _writePos)
                {
                    _readPos = 0;
                    _writePos = 0;
                }
                else if (_recvBuffer.size() - _writePos < 1024)
                {
                    size_t remaining = _writePos - _readPos;
                    ::memmove(&_recvBuffer[0], &_recvBuffer[_readPos], remaining);
                    _readPos = 0;
                    _writePos = remaining;
                }

                RecvLoop();
            }
        );
    }

    void SendLoop()
    {
        if (!_socket.is_open() || !g_IsRunning || _sendStopped)
            return;

        // =========================================================
        // [Latency Measurement] Embed timestamp in packet payload
        // =========================================================
        auto now = std::chrono::steady_clock::now();
        int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

        // Payload: [timestamp (8 bytes)] + "Hello"
        static const char *msg = "Hello";
        size_t payloadSize = sizeof(int64_t) + strlen(msg);
        uint16_t packetSize = static_cast<uint16_t>(sizeof(Share::PacketHeader) + payloadSize);

        if (_sendBuf.size() < packetSize)
            _sendBuf.resize(packetSize);

        Share::PacketHeader *header = reinterpret_cast<Share::PacketHeader *>(_sendBuf.data());
        header->size = packetSize;
        header->id = Share::PacketType::PKT_C_ECHO;

        // Copy timestamp + message
        uint8_t *payload = _sendBuf.data() + sizeof(Share::PacketHeader);
        std::memcpy(payload, &nowUs, sizeof(int64_t));
        std::memcpy(payload + sizeof(int64_t), msg, strlen(msg));
        // =========================================================

        boost::asio::async_write(
            _socket,
            boost::asio::buffer(_sendBuf, packetSize),
            [this, self = shared_from_this()](const boost::system::error_code &ec, size_t)
            {
                if (!ec)
                {
                    g_SendCount++;
                    SendLoop();
                }
            }
        );
    }

private:
    tcp::socket _socket;
    tcp::resolver _resolver;

    std::vector<uint8_t> _recvBuffer;
    size_t _writePos = 0;
    size_t _readPos = 0;

    std::vector<uint8_t> _sendBuf;

    bool _sendStopped = false;
};

int main(int argc, char *argv[])
{
    int clientCount = 500;
    int durationSeconds = 60;

    if (argc > 1)
        clientCount = std::stoi(argv[1]);
    if (argc > 2)
        durationSeconds = std::stoi(argv[2]);

    boost::asio::io_context io_context;

    std::cout << "========================================" << std::endl;
    std::cout << " Starting Stress Test (with Latency)" << std::endl;
    std::cout << " Client Count: " << clientCount << std::endl;
    std::cout << " Duration: " << durationSeconds << "s" << std::endl;
    std::cout << "========================================" << std::endl;

    std::vector<std::shared_ptr<ClientSession>> sessions;
    for (int i = 0; i < clientCount; ++i)
    {
        auto session = std::make_shared<ClientSession>(io_context);
        session->Start("127.0.0.1", "9000");
        sessions.push_back(session);
        if (i % 50 == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuard(io_context.get_executor());
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i)
    {
        threads.emplace_back(
            [&io_context]()
            {
                io_context.run();
            }
        );
    }

    auto startTime = std::chrono::steady_clock::now();
    long long totalSent = 0;
    long long totalRecv = 0;

    for (int i = 0; i < durationSeconds; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        long long sent = g_SendCount.exchange(0);
        long long recv = g_RecvCount.exchange(0);
        totalSent += sent;
        totalRecv += recv;

        // [Latency Stats] 매초 출력
        long long samples = g_LatencySamples.load();
        double avgLatMs = 0;
        if (samples > 0)
        {
            avgLatMs = (double)g_TotalLatencyUs.load() / samples / 1000.0;
        }

        std::cout << "[Sec " << i + 1 << "] Connected: " << g_ConnectedCount << " | Send: " << sent
                  << " | Recv: " << recv << " | AvgLat: " << avgLatMs << "ms" << std::endl;
    }

    // [Phase 1] Stop Sending
    g_IsRunning = false;
    std::cout << "\n[Phase 1] Stopping sends... Recv continues." << std::endl;
    for (auto &session : sessions)
    {
        session->StopSending();
    }

    // [Phase 2] Grace Period
    int zeroRecvCount = 0;
    for (int i = 0; i < 30; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        long long sent = g_SendCount.exchange(0);
        totalSent += sent;
        long long recv = g_RecvCount.exchange(0);
        totalRecv += recv;
        std::cout << "[Grace " << i + 1 << "] Recv: " << recv << std::endl;

        if (recv == 0)
            zeroRecvCount++;
        else
            zeroRecvCount = 0;

        if (zeroRecvCount >= 2)
            break;
    }

    // [Phase 3] Close Sockets
    std::cout << "[Phase 3] Closing sockets..." << std::endl;
    for (auto &session : sessions)
    {
        session->Close();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // [Phase 4] Stop IO
    workGuard.reset();
    io_context.stop();
    for (auto &t : threads)
        t.join();

    // Final Report
    double avgSend = (double)totalSent / durationSeconds;
    double avgRecv = (double)totalRecv / durationSeconds;

    long long samples = g_LatencySamples.load();
    double avgLatMs = samples > 0 ? (double)g_TotalLatencyUs.load() / samples / 1000.0 : 0;
    double minLatMs = g_MinLatencyUs.load() == LLONG_MAX ? 0 : g_MinLatencyUs.load() / 1000.0;
    double maxLatMs = g_MaxLatencyUs.load() / 1000.0;

    std::cout << "\n========================================" << std::endl;
    std::cout << " Test Finished" << std::endl;
    std::cout << " Raw Sent (OS Buffer): " << totalSent << " (Avg: " << avgSend << "/s)" << std::endl;
    std::cout << " Confirmed (=Recv):    " << totalRecv << " (Avg: " << avgRecv << "/s)" << std::endl;
    std::cout << " Loss (Shutdown):      " << (totalSent - totalRecv) << " ("
              << (totalSent > 0 ? (double)(totalSent - totalRecv) / totalSent * 100 : 0) << "%)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << " Latency (RTT):" << std::endl;
    std::cout << "   Samples: " << samples << std::endl;
    std::cout << "   Avg:     " << avgLatMs << " ms" << std::endl;
    std::cout << "   Min:     " << minLatMs << " ms" << std::endl;
    std::cout << "   Max:     " << maxLatMs << " ms" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
