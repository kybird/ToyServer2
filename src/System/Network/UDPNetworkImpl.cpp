#include "System/Network/UDPNetworkImpl.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ILog.h"
#include "System/ISession.h"
#include "System/Network/UDPEndpointRegistry.h"
#include "System/Network/UDPLimits.h"
#include "System/Network/UDPSendContextPool.h"
#include "System/Pch.h"
#include "System/Session/UDPSession.h"

namespace System {

UDPNetworkImpl::UDPNetworkImpl(boost::asio::io_context &ioContext)
    : _ioContext(ioContext), _socket(_ioContext), _strand(boost::asio::make_strand(_socket.get_executor()))
{
    // 컨텍스트 풀 미리 준비 (1024개)
    UDPSendContextPool::Instance().Prepare(1024);
}

UDPNetworkImpl::~UDPNetworkImpl()
{
    Stop();
}

bool UDPNetworkImpl::Start(uint16_t port)
{
    _isStopping.store(false);
    try
    {
        boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), port);

        _socket.open(endpoint.protocol());
        _socket.set_option(boost::asio::socket_base::reuse_address(false));
        _socket.bind(endpoint);

        LOG_INFO("UDP Network listening on 127.0.0.1:{}", port);

        StartReceive();

        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("UDP Network Start Failed: {}", e.what());
        return false;
    }
}

void UDPNetworkImpl::Stop()
{
    _isStopping.store(true);

    // 스트랜드를 통해 안전하게 소켓 닫기 (송신 중인 작업과의 충돌 방지)
    boost::asio::dispatch(
        _strand,
        [this]()
        {
            boost::system::error_code ec;
            if (_socket.is_open())
            {
                _socket.close(ec);
            }
        }
    );
}

void UDPNetworkImpl::SetRegistry(UDPEndpointRegistry *registry)
{
    _registry = registry;
}

void UDPNetworkImpl::SetDispatcher(IDispatcher *dispatcher)
{
    _dispatcher = dispatcher;
}

bool UDPNetworkImpl::SendTo(const uint8_t *data, size_t length, const boost::asio::ip::udp::endpoint &destination)
{
    if (!_socket.is_open() || _isStopping.load())
    {
        return false;
    }

    try
    {
        _socket.async_send_to(
            boost::asio::buffer(data, length),
            destination,
            [](const boost::system::error_code &error, size_t /*bytesSent*/)
            {
                if (error)
                {
                    LOG_ERROR("Legacy UDP Send Error: {}", error.message());
                }
            }
        );
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Legacy UDP Send Exception: {}", e.what());
        return false;
    }
}

void UDPNetworkImpl::AsyncSend(
    const boost::asio::ip::udp::endpoint &destination, uint8_t tag, uint64_t sessionId, uint128_t udpToken,
    PacketMessage *payload, uint16_t payloadLen
)
{
    // [Safety 1] 오버사이즈 체크 (단편화 방지)
    if (payloadLen > UDP_MAX_APP_BYTES)
    {
        _oversizeDrops.fetch_add(1, std::memory_order_relaxed);

        // 5초 단위 Rate-limited 로깅 (CAS 사용)
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        auto lastMs = _lastLogMs.load(std::memory_order_relaxed);
        if (nowMs > lastMs + 5000)
        {
            if (_lastLogMs.compare_exchange_strong(lastMs, nowMs))
            {
                if (tag == UDPTransportHeader::TAG_KCP)
                    LOG_ERROR("UDP KCP Oversize: {} bytes dropped.", payloadLen);
                else
                    LOG_WARN("UDP Raw Oversize: {} bytes dropped.", payloadLen);
            }
        }

        // 즉시 해제 (소유권 정리)
        MessagePool::Free(payload);
        return;
    }

    // [Safety 2] 컨텍스트 풀 확인
    UDPSendContext *ctx = UDPSendContextPool::Instance().Acquire();
    if (!ctx)
    {
        // 풀 부족 시 드랍
        MessagePool::Free(payload);
        return;
    }

    // 컨텍스트 데이터 채우기
    ctx->destination = destination;
    ctx->payload = payload;
    ctx->payloadLen = payloadLen;

    // 헤더 인코딩 (Packed Struct 대신 안전한 오프셋 memcpy)
    std::memcpy(ctx->headerBytes.data() + 0, &tag, sizeof(tag));
    std::memcpy(ctx->headerBytes.data() + 1, &sessionId, sizeof(sessionId));
    std::memcpy(ctx->headerBytes.data() + 9, &udpToken, sizeof(udpToken));

    // [Strand] 동시 송신 호출 순서 보장
    boost::asio::post(
        _strand,
        [this, ctx]()
        {
            if (!_socket.is_open() || _isStopping.load())
            {
                // 스트랜드 내부 실패 시 정리
                MessagePool::Free(ctx->payload);
                ctx->payload = nullptr;
                ctx->payloadLen = 0;
                UDPSendContextPool::Instance().Release(ctx);
                return;
            }

            // Scatter-Gather 송신: 헤더(Context 고정 필드) + 페이로드(PacketMessage 본문)
            std::array<boost::asio::const_buffer, 2> buffers = {
                boost::asio::buffer(ctx->headerBytes), boost::asio::buffer(ctx->payload->Payload(), ctx->payloadLen)
            };

            // [Exception Safety] async_send_to 시작 시 예외 발생 시 정리
            try
            {
                _socket.async_send_to(
                    buffers,
                    ctx->destination,
                    [ctx](const boost::system::error_code &error, size_t /*bytesSent*/)
                    {
                        // 완료 후 무조건 페이로드 해제 및 컨텍스트 반납
                        MessagePool::Free(ctx->payload);

                        ctx->payload = nullptr;
                        ctx->payloadLen = 0;
                        UDPSendContextPool::Instance().Release(ctx);

                        if (error && error != boost::asio::error::operation_aborted)
                        {
                            LOG_ERROR("UDP AsyncSend Error: {}", error.message());
                        }
                    }
                );
            }
            catch (const std::exception &e)
            {
                // async_send_to 시작 실패 시 즉시 정리
                LOG_ERROR("UDP AsyncSend Initiation Failed: {}", e.what());
                MessagePool::Free(ctx->payload);
                ctx->payload = nullptr;
                ctx->payloadLen = 0;
                UDPSendContextPool::Instance().Release(ctx);
            }
        }
    );
}

void UDPNetworkImpl::StartReceive()
{
    if (!_socket.is_open())
        return;

    _socket.async_receive_from(
        boost::asio::buffer(_receiveBuffer),
        _senderEndpoint,
        [this](const boost::system::error_code &error, size_t bytesReceived)
        {
            HandleReceive(error, bytesReceived, _senderEndpoint);
        }
    );
}

void UDPNetworkImpl::HandleReceive(
    const boost::system::error_code &error, size_t bytesReceived, const boost::asio::ip::udp::endpoint &senderEndpoint
)
{
    if (_isStopping.load())
        return;

    if (error)
    {
        if (error != boost::asio::error::operation_aborted)
        {
            LOG_ERROR("UDP Receive Error: {}", error.message());
        }
        return;
    }

    if (bytesReceived > UDPTransportHeader::SIZE)
    {
        if (_registry != nullptr && _dispatcher != nullptr)
        {
            const UDPTransportHeader *header = reinterpret_cast<const UDPTransportHeader *>(_receiveBuffer.data());
            if (!header->IsValid())
            {
                StartReceive();
                return;
            }

            ISession *session = _registry->Find(senderEndpoint);
            if (!session)
            {
                session = _registry->GetEndpointByToken(header->udpToken);
            }

            if (!session)
            {
                StartReceive();
                return;
            }

            _registry->UpdateActivity(senderEndpoint);

            const uint8_t *packetData = _receiveBuffer.data() + UDPTransportHeader::SIZE;
            size_t packetLength = bytesReceived - UDPTransportHeader::SIZE;

            UDPSession *udpSession = dynamic_cast<UDPSession *>(session);
            if (udpSession)
            {
                udpSession->HandleData(packetData, packetLength, header->IsKCP());
            }
        }
    }

    StartReceive();
}

} // namespace System
