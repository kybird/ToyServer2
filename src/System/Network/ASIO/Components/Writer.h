#pragma once
#include "System/Pch.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <memory>
#include <vector>

namespace System {

class AsioSession;

class Writer {
public:
    void Init(std::shared_ptr<boost::asio::ip::tcp::socket> socket, AsioSession *owner);

    // [최적화] 복사 비용 없이 소유권만 이동 (move)
    void Send(std::shared_ptr<std::vector<uint8_t>> packet);

private:
    void Flush();
    void OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred);

private:
    std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    AsioSession *_owner = nullptr;

    // 1. Lock-Free Queue
    moodycamel::ConcurrentQueue<std::shared_ptr<std::vector<uint8_t>>> _sendQueue;

    // 2. Atomic Flag (가장 빠름)
    std::atomic<bool> _isSending = false;

    // 3. [추가] 패킷들을 하나로 합칠 거대 버퍼 (Linearization Buffer)
    // 매번 할당하지 않고 capacity를 유지하며 재사용합니다.
    std::vector<uint8_t> _linearBuffer;

    // 4. Boost.Asio 전송용 버퍼 래퍼
    std::vector<boost::asio::const_buffer> _gatherBuffer; // async_write에 넘겨줄 용도

    // [Monitoring]
    size_t _statFlushCount = 0;     // Number of Flush calls
    size_t _statTotalItemCount = 0; // Total number of items processed
    size_t _statMaxBatch = 0;       // Max batch size seen
    std::chrono::steady_clock::time_point _lastStatTime = std::chrono::steady_clock::now();
};

} // namespace System