#pragma once

#include "System/Session/UDPSession.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ILog.h"
#include "System/Network/GenerateUDPToken.h"
#include "System/Network/UDPNetworkImpl.h"
#include "System/Network/UDPTransportHeader.h"
#include "System/Packet/IPacket.h"
#include "System/Pch.h"
#include "System/Session/UDP/KCPAdapter.h"

#include <cstdint>
#include <functional>

namespace System {

struct UDPSessionImpl
{
    boost::asio::ip::udp::endpoint endpoint;
    std::chrono::steady_clock::time_point lastActivity;
    UDPNetworkImpl *network = nullptr;
    uint128_t udpToken = 0;
    std::unique_ptr<IKCPAdapter> kcp;
};

UDPSession::UDPSession() : _impl(std::make_unique<UDPSessionImpl>())
{
}

UDPSession::~UDPSession()
{
}

void UDPSession::Reset(
    std::shared_ptr<void> socketVoidPtr, uint64_t sessionId, IDispatcher *dispatcher,
    const boost::asio::ip::udp::endpoint &endpoint
)
{
    Session::Reset();

    _id = sessionId;
    _dispatcher = dispatcher;
    _connected.store(true);

    _impl->endpoint = endpoint;
    _impl->lastActivity = std::chrono::steady_clock::now();
    _impl->udpToken = GenerateUDPToken::Generate();

    // KCP 초기화
    _impl->kcp = std::make_unique<KCPAdapter>(static_cast<uint32_t>(sessionId));
    _impl->kcp->SetOutputCallback(
        [this](const char *buf, int len) -> int
        {
            if (_impl->network == nullptr)
                return 0;

            // [Zero-Copy] 풀에서 할당 -> 복사 1회 -> AsyncSend로 소유권 이전
            auto *msg = MessagePool::AllocatePacket(static_cast<uint16_t>(len));
            if (!msg)
            {
                LOG_ERROR("KCP Output Failed: MessagePool Exhausted");
                return -1;
            }

            std::memcpy(msg->Payload(), buf, len);

            // AsyncSend가 메시지를 해제할 책임을 가짐
            _impl->network->AsyncSend(
                _impl->endpoint, UDPTransportHeader::TAG_KCP, _id, _impl->udpToken, msg, static_cast<uint16_t>(len)
            );

            return 0;
        }
    );

    LOG_INFO("[UDPSession] Session {} reset for endpoint {}:{}", _id, endpoint.address().to_string(), endpoint.port());
}

void UDPSession::OnRecycle()
{
    _impl->network = nullptr;
    _impl->endpoint = {};
    _impl->kcp.reset();
    _connected.store(false);

    // 잔여 큐 정리
    PacketMessage *msg;
    while (_sendQueue.try_dequeue(msg))
    {
        MessagePool::Free(msg);
    }
}

const boost::asio::ip::udp::endpoint &UDPSession::GetEndpoint() const
{
    return _impl->endpoint;
}

void UDPSession::UpdateActivity()
{
    _impl->lastActivity = std::chrono::steady_clock::now();
}

const std::chrono::steady_clock::time_point &UDPSession::GetLastActivity() const
{
    return _impl->lastActivity;
}

void UDPSession::SetNetwork(UDPNetworkImpl *network)
{
    _impl->network = network;
}

UDPNetworkImpl *UDPSession::GetNetwork() const
{
    return _impl->network;
}

void UDPSession::SetUdpToken(uint128_t token)
{
    _impl->udpToken = token;
}

uint128_t UDPSession::GetUdpToken() const
{
    return _impl->udpToken;
}

void UDPSession::HandleData(const uint8_t *data, size_t length, bool isKCP)
{
    UpdateActivity();

    if (isKCP && _impl->kcp != nullptr)
    {
        _impl->kcp->Input(data, static_cast<int>(length));

        uint8_t buffer[2048];
        int receivedSize;
        while ((receivedSize = _impl->kcp->Recv(buffer, sizeof(buffer))) > 0)
        {
            if (_dispatcher != nullptr)
            {
                auto *msg = MessagePool::AllocatePacket(static_cast<uint16_t>(receivedSize));
                if (msg != nullptr)
                {
                    msg->type = MessageType::NETWORK_DATA;
                    msg->sessionId = _id;
                    msg->session = this;
                    std::memcpy(msg->Payload(), buffer, receivedSize);

                    IncRef();
                    _dispatcher->Post(msg);
                }
            }
        }
    }
    else
    {
        if (_dispatcher != nullptr)
        {
            auto *msg = MessagePool::AllocatePacket(static_cast<uint16_t>(length));
            if (msg != nullptr)
            {
                msg->type = MessageType::NETWORK_DATA;
                msg->sessionId = _id;
                msg->session = this;
                std::memcpy(msg->Payload(), data, length);

                IncRef();
                _dispatcher->Post(msg);
            }
        }
    }
}

void UDPSession::OnConnect()
{
    LOG_INFO(
        "[UDPSession] Session {} connected from {}:{}",
        _id,
        _impl->endpoint.address().to_string(),
        _impl->endpoint.port()
    );

    _connected.store(true);
}

void UDPSession::OnDisconnect()
{
    LOG_INFO(
        "[UDPSession] Session {} disconnected from {}:{}",
        _id,
        _impl->endpoint.address().to_string(),
        _impl->endpoint.port()
    );

    _connected.store(false);
}

void UDPSession::Close()
{
    OnDisconnect();
}

void UDPSession::SendReliable(const IPacket &pkt)
{
    if (!IsConnected())
        return;

    uint16_t size = pkt.GetTotalSize();

    // 최적화: 작은 패킷은 스택 버퍼 사용
    if (size <= 1024)
    {
        uint8_t buffer[1024];
        pkt.SerializeTo(buffer);
        if (_impl->kcp)
        {
            _impl->kcp->Send(buffer, size);
            _impl->kcp->Update(
                static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch()
                )
                                          .count())
            );
        }
    }
    else
    {
        std::vector<uint8_t> buffer(size);
        pkt.SerializeTo(buffer.data());
        if (_impl->kcp)
        {
            _impl->kcp->Send(buffer.data(), size);
            _impl->kcp->Update(
                static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch()
                )
                                          .count())
            );
        }
    }
}

void UDPSession::SendUnreliable(const IPacket &pkt)
{
    // 큐에 쌓고 Flush()에서 AsyncSend로 처리
    SendPacket(pkt);
}

void UDPSession::UpdateKCP(uint32_t currentMs)
{
    if (_impl->kcp != nullptr)
    {
        _impl->kcp->Update(currentMs);
    }
}

void UDPSession::Flush()
{
    if (_impl->network == nullptr)
    {
        PacketMessage *msg;
        while (_sendQueue.try_dequeue(msg))
        {
            MessagePool::Free(msg);
        }
        return;
    }

    // [Loop Pattern]
    // 1. 현재 큐에 있는 모든 메시지를 소진합니다.
    // 2. _isSending 플래그를 해제합니다.
    // 3. 다시 큐를 확인하여 지연 패킷(Straggler)이 있는지 확인합니다.
    // 4. 있을 경우 Re-enqueue 후 소유권을 다시 획득하여 루프를 재시작합니다.

    while (true)
    {
        PacketMessage *msg;
        while (_sendQueue.try_dequeue(msg))
        {
            // AsyncSend로 소유권 이전. 여기서 직접 해제하지 않음.
            _impl->network->AsyncSend(
                _impl->endpoint, UDPTransportHeader::TAG_RAW_UDP, _id, _impl->udpToken, msg, msg->length
            );
        }

        _isSending.store(false);

        if (_sendQueue.try_dequeue(msg))
        {
            // 뒤늦게 들어온 패킷 처리 (Order 보존을 위해 무조건 다시 넣음)
            _sendQueue.enqueue(msg);

            bool expected = false;
            if (_isSending.compare_exchange_strong(expected, true))
            {
                // 소유권 재획득 성공 -> 루프 계속
                continue;
            }
            else
            {
                // 다른 스레드가 이미 Flush를 시작함
                break;
            }
        }

        break;
    }
}

} // namespace System
