#pragma once

#include "System/Network/UDPTransportHeader.h"
#include "System/Pch.h"
#include <atomic>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

namespace System {

class IDispatcher;
class UDPEndpointRegistry;
class PacketMessage;

/**
 * @brief UDP 네트워크 구현 클래스
 * 비동기 송신(AsyncSend)을 통해 제로 할당 및 스레드 안전한 송신을 지원합니다.
 */
class UDPNetworkImpl
{
public:
    explicit UDPNetworkImpl(boost::asio::io_context &ioContext);
    ~UDPNetworkImpl();

    bool Start(uint16_t port);
    void Stop();

    void SetRegistry(UDPEndpointRegistry *registry);
    void SetDispatcher(IDispatcher *dispatcher);

    /**
     * @brief [Legacy] UDP 데이터 직접 송신
     * @deprecated AsyncSend 사용을 권장합니다.
     */
    bool SendTo(const uint8_t *data, size_t length, const boost::asio::ip::udp::endpoint &destination);

    /**
     * @brief 제로 할당 비동기 UDP 송신
     *
     * @param destination 목적지 엔드포인트
     * @param tag 패킷 태그 (Raw UDP 또는 KCP)
     * @param sessionId 세션 ID
     * @param udpToken 전송용 보안 토큰
     * @param payload 메시지 페이로드 객체
     * @param payloadLen 실제 송신 데이터 길이
     *
     * @note 소유권 규칙:
     * - 전달받은 `payload`의 소유권을 1개 가져옵니다.
     * - 송신 시작에 실패(Oversize 등)하면 즉시 호출자 스레드에서 해제합니다.
     * - 성공적으로 strand에 게시되면, 송신 완료 핸들러에서만 해제합니다.
     */
    void AsyncSend(
        const boost::asio::ip::udp::endpoint &destination, uint8_t tag, uint64_t sessionId, uint128_t udpToken,
        PacketMessage *payload, uint16_t payloadLen
    );

    /**
     * @brief 오버사이즈 패킷 드랍 횟수 조회
     * @return 누적 드랍 횟수
     */
    uint64_t GetOversizeDrops() const { return _oversizeDrops.load(std::memory_order_relaxed); }

private:
    void StartReceive();
    void HandleReceive(
        const boost::system::error_code &error, size_t bytesReceived,
        const boost::asio::ip::udp::endpoint &senderEndpoint
    );

private:
    boost::asio::io_context &_ioContext;
    boost::asio::ip::udp::socket _socket;
    boost::asio::strand<boost::asio::any_io_executor> _strand;

    boost::asio::ip::udp::endpoint _senderEndpoint;
    UDPEndpointRegistry *_registry = nullptr;
    IDispatcher *_dispatcher = nullptr;
    std::atomic<bool> _isStopping{false};

    // 통계 및 메트릭
    std::atomic<uint64_t> _oversizeDrops{0};
    std::atomic<int64_t> _lastLogMs{0};

    // 수신 버퍼
    std::array<uint8_t, 65536> _receiveBuffer;
};

} // namespace System
