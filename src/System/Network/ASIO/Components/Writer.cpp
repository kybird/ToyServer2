#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/recycling_allocator.hpp>

#include "System/Network/ASIO/AsioSession.h"
#include "System/Network/ASIO/Components/Writer.h"
#include "System/Pch.h"

namespace System {

void Writer::Init(std::shared_ptr<boost::asio::ip::tcp::socket> socket, AsioSession *owner)
{
    _socket = socket;
    _owner = owner;

    // [메모리 최적화]
    // 패킷이 많이 몰릴 것을 대비해 넉넉하게 잡음 (예: 64KB ~ 1MB)
    // 한번 늘어나면 줄어들지 않고 계속 재사용됨 (재할당 비용 0)
    _linearBuffer.reserve(64 * 1024);

    _isSending.store(false);

    // [Cleanup Reuse]
    std::shared_ptr<std::vector<uint8_t>> dummy;
    while (_sendQueue.try_dequeue(dummy))
    {
        // Drain old packets
    }
}

void Writer::Send(std::shared_ptr<std::vector<uint8_t>> packet)
{
    // 1. 큐 삽입
    _sendQueue.enqueue(std::move(packet));

    // 2. 전송 트리거 (CAS 없이 exchange가 가장 빠름)
    if (_isSending.exchange(true) == false)
    {
        Flush();
    }
}

void Writer::Flush()
{
    if (!_socket || !_socket->is_open())
    {
        _isSending.store(false);
        return;
    }

    // [튜닝 포인트]
    // 복사 방식(Linearize)은 배치를 크게 잡을수록 유리합니다.
    // 시스템 콜 1번에 최대한 많이 욱여넣기 위함입니다.
    static const size_t MAX_BATCH_SIZE = 1000;

    // 스택 메모리에 포인터 배열 할당 (힙 할당 X)
    std::shared_ptr<std::vector<uint8_t>> tempItems[MAX_BATCH_SIZE];

    // 1. 큐에서 뭉텅이로 꺼내기
    size_t count = _sendQueue.try_dequeue_bulk(tempItems, MAX_BATCH_SIZE);

    // [INJECT START] Monitoring Logic (Log once per second)
    if (count > 0)
    {
        _statFlushCount++;
        _statTotalItemCount += count;
        if (count > _statMaxBatch)
            _statMaxBatch = count;

        auto now = std::chrono::steady_clock::now();
        if (now - _lastStatTime > std::chrono::seconds(1))
        {
            double avgBatch = (double)_statTotalItemCount / _statFlushCount;

            // Log format: [Writer] Flush Calls: {count}, Avg Batch: {avg}, Max Batch: {max}
            LOG_FILE(
                "[Writer] Flush Calls: {}, Avg Batch: {:.2f}, Max Batch: {}", _statFlushCount, avgBatch, _statMaxBatch
            );

            // Reset stats
            _statFlushCount = 0;
            _statTotalItemCount = 0;
            _statMaxBatch = 0;
            _lastStatTime = now;
        }
    }
    // [INJECT END]

    // 2. 큐가 비었을 때 처리 (좀비 방지 Double Check)
    if (count == 0)
    {
        _isSending.store(false); // 퇴근 도장

        // 뒤돌아보기: 퇴근 직전에 누가 넣었나?
        std::shared_ptr<std::vector<uint8_t>> straggler;
        if (_sendQueue.try_dequeue(straggler))
        {
            bool expected = false;
            // 다시 출근 시도
            if (_isSending.compare_exchange_strong(expected, true))
            {
                // [낙오자 처리]
                // 1개뿐이니 복잡한 병합 없이 바로 전송
                _linearBuffer.clear();

                // 낙오자 데이터 복사
                size_t size = straggler->size();
                if (_linearBuffer.capacity() < size)
                    _linearBuffer.reserve(size);
                _linearBuffer.resize(size);
                ::memcpy(_linearBuffer.data(), straggler->data(), size);

                // 원본 즉시 반납 (풀로 복귀)
                straggler.reset();

                // 전송
                // [Lifetime Safety] Capture self
                std::shared_ptr<AsioSession> self = _owner->shared_from_this();

                boost::asio::async_write(
                    *_socket,
                    boost::asio::buffer(_linearBuffer),
                    boost::asio::bind_allocator(
                        boost::asio::recycling_allocator<void>(),
                        [this, self](const boost::system::error_code &ec, size_t tr)
                        {
                            OnWriteComplete(ec, tr);
                        }
                    )
                );
                return;
            }
            else
            {
                // 다른 스레드가 이미 들어옴 -> 다시 큐에 넣고 퇴근
                _sendQueue.enqueue(std::move(straggler));
            }
        }
        return;
    }

    // ==========================================================
    // [Linearize 구현] 흩어진 패킷을 하나로 합침
    // ==========================================================

    // A. 전체 크기 계산
    size_t totalSize = 0;
    for (size_t i = 0; i < count; ++i)
    {
        totalSize += tempItems[i]->size();
    }

    // B. 버퍼 준비 (Capacity 내에서는 할당 없음)
    _linearBuffer.clear();
    if (_linearBuffer.capacity() < totalSize)
    {
        // 부족하면 1.5배~2배 넉넉히 확장
        _linearBuffer.reserve(std::max(totalSize, _linearBuffer.capacity() * 2));
    }
    // resize는 초기화 비용이 있을 수 있으나, memcpy로 덮어쓰므로
    // 최신 컴파일러는 memset을 생략하기도 함. (가장 안전하고 표준적인 방법)
    _linearBuffer.resize(totalSize);

    // C. 고속 복사 (memcpy loop)
    uint8_t *destPtr = _linearBuffer.data();
    for (size_t i = 0; i < count; ++i)
    {
        size_t pktSize = tempItems[i]->size();

        // 메모리 복사 (L1 캐시 내에서 동작하므로 매우 빠름)
        ::memcpy(destPtr, tempItems[i]->data(), pktSize);
        destPtr += pktSize;

        // [핵심] 복사했으니 원본 패킷은 즉시 반납!
        // async_write가 끝날 때까지 기다릴 필요가 없음.
        // PacketPool 회전율이 비약적으로 상승함.
        tempItems[i].reset();
    }

    // 3. 전송 (OS에는 거대 버퍼 주소 1개만 던짐)
    // [Lifetime Safety] buffer pointer is valid (member of Writer),
    // and writer is member of Session. Capture Session to keep all alive.
    std::shared_ptr<AsioSession> self = _owner->shared_from_this();

    boost::asio::async_write(
        *_socket,
        boost::asio::buffer(_linearBuffer),
        boost::asio::bind_allocator(
            boost::asio::recycling_allocator<void>(),
            [this, self](const boost::system::error_code &ec, size_t bytesTransferred)
            {
                OnWriteComplete(ec, bytesTransferred);
            }
        )
    );
}

void Writer::OnWriteComplete(const boost::system::error_code &ec, size_t bytesTransferred)
{
    if (ec)
    {
        _isSending.store(false);
        return;
    }
    // 완료 후 재진입
    Flush();
}

} // namespace System