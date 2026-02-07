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

    // Initialize KCP
    _impl->kcp = std::make_unique<KCPAdapter>(static_cast<uint32_t>(sessionId));
    _impl->kcp->SetOutputCallback(
        [this](const char *buf, int len) -> int
        {
            if (_impl->network == nullptr) // Fix: boolean comparison
                return 0;

            std::vector<uint8_t> sendBuffer;
            sendBuffer.resize(UDPTransportHeader::SIZE + len);

            UDPTransportHeader *header = reinterpret_cast<UDPTransportHeader *>(sendBuffer.data());
            header->tag = UDPTransportHeader::TAG_KCP;
            header->sessionId = _id;
            header->udpToken = _impl->udpToken;

            std::memcpy(sendBuffer.data() + UDPTransportHeader::SIZE, buf, len);
            _impl->network->SendTo(sendBuffer.data(), sendBuffer.size(), _impl->endpoint);
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

    if (isKCP && _impl->kcp != nullptr) // Fix: boolean comparison
    {
        _impl->kcp->Input(data, static_cast<int>(length));

        // Read all available packets from KCP
        uint8_t buffer[2048];
        int receivedSize;
        while ((receivedSize = _impl->kcp->Recv(buffer, sizeof(buffer))) > 0)
        {
            if (_dispatcher != nullptr) // Fix: boolean comparison
            {
                auto *msg = MessagePool::AllocatePacket(static_cast<uint16_t>(receivedSize));
                if (msg != nullptr) // Fix: boolean comparison
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
        // Raw UDP - Post directly to dispatcher
        if (_dispatcher != nullptr) // Fix: boolean comparison
        {
            auto *msg = MessagePool::AllocatePacket(static_cast<uint16_t>(length));
            if (msg != nullptr) // Fix: boolean comparison
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
    std::vector<uint8_t> buffer(size);
    pkt.SerializeTo(buffer.data());

    if (_impl->kcp != nullptr) // Fix: boolean comparison
    {
        _impl->kcp->Send(buffer.data(), size);
        // Force flush KCP
        _impl->kcp->Update(
            static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now().time_since_epoch()
            )
                                      .count())
        );
    }
}

void UDPSession::SendUnreliable(const IPacket &pkt)
{
    SendPacket(pkt);
}

void UDPSession::UpdateKCP(uint32_t currentMs)
{
    if (_impl->kcp != nullptr) // Fix: boolean comparison
    {
        _impl->kcp->Update(currentMs);
    }
}

void UDPSession::Flush()
{
    if (_impl->network == nullptr) // Fix: boolean comparison
    {
        return;
    }

    PacketMessage *msg;
    while (_sendQueue.try_dequeue(msg))
    {
        const uint16_t packetSize = msg->length;

        std::vector<uint8_t> sendBuffer;
        sendBuffer.resize(UDPTransportHeader::SIZE + packetSize);

        UDPTransportHeader *transportHeader = reinterpret_cast<UDPTransportHeader *>(sendBuffer.data());
        transportHeader->tag = UDPTransportHeader::TAG_RAW_UDP;
        transportHeader->sessionId = _id;
        transportHeader->udpToken = _impl->udpToken;

        std::memcpy(sendBuffer.data() + UDPTransportHeader::SIZE, msg->Payload(), packetSize);

        _impl->network->SendTo(sendBuffer.data(), sendBuffer.size(), _impl->endpoint);

        if (msg->DecRef() == 0)
        {
            if (msg->isPooled)
            {
                // MessagePool::Free(msg);
            }
        }
    }
}

} // namespace System
