#include "System/Session/UDP/KCPWrapper.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include <cstring>
#include <algorithm>

namespace System {

KCPWrapper::KCPWrapper() : _localSequence(0), _remoteSequence(0), _conv(0), _lastUpdate(0)
{
}

KCPWrapper::~KCPWrapper()
{
}

void KCPWrapper::Initialize(uint32_t conv)
{
    _conv = conv;
    _localSequence = 0;
    _remoteSequence = 0;
    _lastUpdate = 0;

    // Clear send window
    for (auto &pkt : _sendWindow)
    {
        pkt.sequence = 0;
        pkt.data.clear();
    }
    _receiveQueue.clear();

    LOG_DEBUG("[KCPWrapper] Initialized with conv={}", conv);
}

int KCPWrapper::Send(const void *data, int length)
{
    if (length <= 0 || length > 1024)
    {
        LOG_ERROR("[KCPWrapper] Invalid send length: {}", length);
        return -1;
    }

    Packet pkt;
    pkt.sequence = _localSequence++;
    pkt.data.assign(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + length);

    // Find slot in send window
    size_t slot = _localSequence % SEND_WINDOW_SIZE;
    _sendWindow[slot] = pkt;

    LOG_DEBUG("[KCPWrapper] Queued packet seq={} size={}", _localSequence, length);

    return length;
}

int KCPWrapper::Input(const void *data, int length)
{
    if (length < 4)
    {
        LOG_ERROR("[KCPWrapper] Packet too small: {}", length);
        return -1;
    }

    // Read sequence number from packet
    uint32_t seq = *reinterpret_cast<const uint32_t *>(data);

    // Simple reordering: if sequence > expected, buffer for later delivery
    if (seq == _remoteSequence)
    {
        // Expected packet - process immediately
        Packet pkt;
        pkt.sequence = seq;
        pkt.data.assign(static_cast<const uint8_t *>(data) + 4,
                       static_cast<const uint8_t *>(data) + length);

        _receiveQueue.push_back(pkt);
        _remoteSequence++;

        LOG_DEBUG("[KCPWrapper] Received packet seq={} size={}", seq, length - 4);
    }
    else if (seq > _remoteSequence)
    {
        // Future packet - queue for now (simplified)
        // In real implementation, would use reassembly buffer
        Packet pkt;
        pkt.sequence = seq;
        pkt.data.assign(static_cast<const uint8_t *>(data) + 4,
                       static_cast<const uint8_t *>(data) + length);

        _receiveQueue.push_back(pkt);

        LOG_DEBUG("[KCPWrapper] Received future packet seq={} size={}", seq, length - 4);
    }
    else
    {
        // Old/duplicate packet - discard
        LOG_DEBUG("[KCPWrapper] Discarding old packet seq={} (expected {})", seq, _remoteSequence);
    }

    // Trim receive buffer
    while (_receiveQueue.size() > RECEIVE_BUFFER_SIZE)
    {
        _receiveQueue.pop_front();
    }

    return length;
}

void KCPWrapper::Update(uint32_t current)
{
    uint32_t elapsed = current - _lastUpdate;

    // Only update periodically (10ms)
    if (elapsed >= UPDATE_INTERVAL)
    {
        _lastUpdate = current;

        // Simple retransmission logic
        // In real KCP, this would check for ACKs and resend unacked packets
        // Here we just log for demonstration
        LOG_DEBUG("[KCPWrapper] Update elapsed={}ms", elapsed);
    }
}

int KCPWrapper::Recv(uint8_t *buffer, int maxSize)
{
    if (_receiveQueue.empty())
    {
        return 0;
    }

    // Remove first packet from queue
    Packet pkt = _receiveQueue.front();
    _receiveQueue.pop_front();

    if (pkt.data.empty())
    {
        return 0;
    }

    // Check buffer size
    int copySize = (pkt.data.size() < static_cast<size_t>(maxSize))
                    ? static_cast<int>(pkt.data.size())
                    : maxSize;

    // Copy header + sequence number (4 bytes)
    uint32_t seq = pkt.sequence;
    memcpy(buffer, &seq, 4);
    memcpy(buffer + 4, pkt.data.data(), copySize);

    LOG_DEBUG("[KCPWrapper] Returning packet seq={} size={}", seq, copySize);

    return 4 + copySize;
}

int KCPWrapper::Output(uint8_t *buffer, int maxSize)
{
    // Simple output: return all packets in send window in order
    // This is a very basic implementation
    // In real KCP, this would use congestion control and ACKs

    int totalSize = 0;

    for (size_t i = 0; i < _sendWindow.size(); ++i)
    {
        const Packet &pkt = _sendWindow[i];

        if (pkt.sequence == 0 || pkt.data.empty())
        {
            continue;
        }

        // Check if we've received ACK for this packet
        // Simplified: just send all packets <= remote sequence
        if (pkt.sequence <= _remoteSequence)
        {
            continue;
        }

        // Add header (4 bytes for sequence number)
        int packetSize = 4 + static_cast<int>(pkt.data.size());

        if (totalSize + packetSize > maxSize)
        {
            break;
        }

        // Copy sequence number
        memcpy(buffer + totalSize, &pkt.sequence, 4);
        // Copy data
        memcpy(buffer + totalSize + 4, pkt.data.data(), pkt.data.size());

        totalSize += packetSize;
    }

    LOG_DEBUG("[KCPWrapper] Output {} bytes in {} packets", totalSize, _sendWindow.size());

    return totalSize;
}

} // namespace System
