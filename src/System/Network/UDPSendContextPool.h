#pragma once

#include "System/Dispatcher/IMessage.h"
#include "System/Network/UDPLimits.h"
#include "System/Pch.h"
#include <boost/asio/ip/udp.hpp>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <mutex>
#include <vector>


namespace System {

/**
 * @brief 단일 UDP 비동기 송신 작업을 위한 컨텍스트
 * 힙 할당 없이 재사용됩니다.
 */
struct UDPSendContext
{
    // 전송 헤더 버퍼 (memcpy로 직접 인코딩)
    std::array<uint8_t, UDP_TRANSPORT_HEADER_BYTES> headerBytes;

    // 페이로드 참조 (AsyncSend 완료 전까지 소유권 유지)
    PacketMessage *payload = nullptr;
    uint16_t payloadLen = 0;

    // 목적지 주소
    boost::asio::ip::udp::endpoint destination;
};

/**
 * @brief UDPSendContext을 관리하는 고정 크기 풀
 * moodycamel::ConcurrentQueue를 사용하여 스레드 안전(MPMC)하게 동작합니다.
 */
class UDPSendContextPool
{
public:
    static UDPSendContextPool &Instance();

    // 초기 크기만큼 컨텍스트를 미리 할당합니다.
    void Prepare(size_t poolSize);

    // 풀에서 컨텍스트를 하나 가져옵니다. 부족할 경우 nullptr을 반환합니다.
    UDPSendContext *Acquire();

    // 사용이 끝난 컨텍스트를 풀에 반납합니다.
    // 반납 전 반드시 payload를 nullptr로 초기화해야 합니다.
    void Release(UDPSendContext *ctx);

private:
    UDPSendContextPool() = default;
    ~UDPSendContextPool();

    std::vector<UDPSendContext *> _allContexts;
    moodycamel::ConcurrentQueue<UDPSendContext *> _pool;

    std::mutex _initMutex;
    bool _initialized = false;
};

} // namespace System
