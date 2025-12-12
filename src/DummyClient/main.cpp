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

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
  ClientSession(boost::asio::io_context &io_context)
      : _socket(io_context), _resolver(io_context) {
    // 수신 버퍼 넉넉히 (64KB)
    _recvBuffer.resize(65536);
  }

  void Start(const std::string &host, const std::string &port) {
    auto endpoints = _resolver.resolve(host, port);
    boost::asio::async_connect(
        _socket, endpoints,
        [this, self = shared_from_this()](const boost::system::error_code &ec,
                                          tcp::endpoint) {
          if (!ec) {
            g_ConnectedCount++;

            // [최적화] 소켓 버퍼 크기 늘리기 (OS 레벨)
            boost::asio::socket_base::receive_buffer_size option(1024 *
                                                                 1024); // 1MB
            _socket.set_option(option);

            // Nagle 끄기 (반응 속도 향상)
            tcp::no_delay noDelay(true);
            _socket.set_option(noDelay);

            SendLoop();
            RecvLoop(); // 바뀐 수신 함수 호출
          }
        });
  }

private:
  // [기존] ReadHeader -> ReadBody (삭제)
  // [신규] 뭉텅이로 읽어서 루프로 처리
  void RecvLoop() {
    _socket.async_read_some(
        boost::asio::buffer(_recvBuffer.data() + _writePos,
                            _recvBuffer.size() - _writePos),
        [this, self = shared_from_this()](const boost::system::error_code &ec,
                                          size_t bytesTransferred) {
          if (ec)
            return; // 연결 끊김 등

          // 1. 커서 이동
          _writePos += bytesTransferred;

          // 2. 패킷 처리 루프 (서버와 동일한 로직!)
          while (_writePos - _readPos >= sizeof(Share::PacketHeader)) {
            // 헤더 확인
            Share::PacketHeader *header =
                reinterpret_cast<Share::PacketHeader *>(&_recvBuffer[_readPos]);

            // 패킷 크기 검증
            if (header->size > 1024 * 10) {
              // 에러 처리: 너무 큰 패킷 (Close)
              return;
            }

            // 아직 바디가 다 안 왔으면 대기
            if (_writePos - _readPos < header->size) {
              break;
            }

            // --- [패킷 처리 완료] ---
            g_RecvCount++; // 카운트 증가
            // -----------------------

            // 읽은 만큼 포지션 이동
            _readPos += header->size;
          }

          // 3. 버퍼 정리 (남은 데이터 앞으로 당기기)
          // 만약 읽을 데이터가 없으면 커서 초기화 (가장 빠름)
          if (_readPos == _writePos) {
            _readPos = 0;
            _writePos = 0;
          }
          // 만약 버퍼가 꽉 찼거나 남은 공간이 부족하면 앞으로 당김 (Memmove)
          else if (_recvBuffer.size() - _writePos < 1024) {
            size_t remaining = _writePos - _readPos;
            ::memmove(&_recvBuffer[0], &_recvBuffer[_readPos], remaining);
            _readPos = 0;
            _writePos = remaining;
          }

          // 4. 다시 수신 대기
          RecvLoop();
        });
  }

  void SendLoop() {
    if (!_socket.is_open() || !g_IsRunning)
      return;

    // ... 패킷 생성 로직 (기존과 동일) ...
    // (주의: 테스트할 때 Sleep을 0ms로 하거나 제거하면 엄청난 속도로 나감)

    // 패킷 생성 비용을 아끼기 위해 멤버변수로 미리 만들어두고 재사용 추천
    // 여기서는 흐름상 기존 코드 유지
    static std::string payload =
        "Hello Dummy Client";            // static으로 한 번만 생성
    static std::vector<uint8_t> sendBuf; // 재사용

    uint16_t size = static_cast<uint16_t>(sizeof(Share::PacketHeader) + payload.size());
    if (sendBuf.size() < size)
      sendBuf.resize(size);

    Share::PacketHeader *header = reinterpret_cast<Share::PacketHeader *>(sendBuf.data());
    header->size = size;
    header->id = Share::PacketType::PKT_C_ECHO;
    memcpy(sendBuf.data() + sizeof(Share::PacketHeader), payload.data(),
           payload.size());

    g_SendCount++;

    boost::asio::async_write(
        _socket,
        boost::asio::buffer(sendBuf, size), // 전체 벡터가 아니라 size만큼만
        [this, self = shared_from_this()](const boost::system::error_code &ec,
                                          size_t) {
          if (!ec) {

            // 스트레스 테스트: Sleep 없애거나 1ms
            SendLoop();
          } else {
            g_SendCount--;

            // 로그 출력 (aborted 제외)
            if (ec != boost::asio::error::operation_aborted) {
              // std::cerr << "Send Fail" << std::endl;
            }
          }
        });
  }

private:
  tcp::socket _socket;
  tcp::resolver _resolver;

  // [수신 버퍼 관리용 변수]
  std::vector<uint8_t> _recvBuffer;
  size_t _writePos = 0;
  size_t _readPos = 0;
};

int main(int argc, char *argv[]) {
  int clientCount = 500;
  int durationSeconds = 10;

  if (argc > 1)
    clientCount = std::stoi(argv[1]);
  if (argc > 2)
    durationSeconds = std::stoi(argv[2]);

  boost::asio::io_context io_context;

  std::cout << "========================================" << std::endl;
  std::cout << " Starting Stress Test" << std::endl;
  std::cout << " Client Count: " << clientCount << std::endl;
  std::cout << " Duration: " << durationSeconds << "s" << std::endl;
  std::cout << "========================================" << std::endl;

  std::vector<std::shared_ptr<ClientSession>> sessions;
  for (int i = 0; i < clientCount; ++i) {
    auto session = std::make_shared<ClientSession>(io_context);
    session->Start("127.0.0.1", "9000");
    sessions.push_back(session);
    if (i % 50 == 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      workGuard(io_context.get_executor());
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&io_context]() { io_context.run(); });
  }

  auto startTime = std::chrono::steady_clock::now();
  long long totalSent = 0;
  long long totalRecv = 0;

  for (int i = 0; i < durationSeconds; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    long long sent = g_SendCount.exchange(0);
    long long recv = g_RecvCount.exchange(0);
    totalSent += sent;
    totalRecv += recv;

    std::cout << "[Sec " << i + 1 << "] Connected: " << g_ConnectedCount
              << " | Send: " << sent << " | Recv: " << recv << std::endl;
  }

  // Stop Sending
  g_IsRunning = false;
  std::cout << "\n[Stopping sends... Waiting for remaining packets (2s)...]"
            << std::endl;

  // Grace Period
  for (int i = 0; i < 30; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    long long sent = g_SendCount.exchange(0);
    totalSent += sent;
    long long recv = g_RecvCount.exchange(0);
    totalRecv += recv;
    std::cout << "[Grace " << i + 1 << "] Recv: " << recv << std::endl;

    if (sent == 0 && recv == 0)
      break;
  }

  // Stop IO
  workGuard.reset();
  io_context.stop();
  for (auto &t : threads)
    t.join();

  double avgSend = (double)totalSent / durationSeconds;
  double avgRecv = (double)totalRecv / durationSeconds;

  std::cout << "\n========================================" << std::endl;
  std::cout << " Test Finished" << std::endl;
  std::cout << " Total Sent: " << totalSent << " (Avg: " << avgSend << "/s)"
            << std::endl;
  std::cout << " Total Recv: " << totalRecv << " (Avg: " << avgRecv << "/s)"
            << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
