#include "System/Session/UDP/IKCPWrapper.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include <cstring>
#include <deque>

namespace System {

/**
 * @brief Minimal ARQ implementation.
 * Simple reliable UDP protocol with basic retransmission and ordering.
 */
class KCPWrapper : public IKCPWrapper
{
public:
    KCPWrapper();
    ~KCPWrapper() override;

    // IKCPWrapper implementation
    void Initialize(uint32_t conv) override;
    int Send(const void *data, int length) override;
    int Input(const void *data, int length) override;
    void Update(uint32_t current) override;
    int Recv(uint8_t *buffer, int maxSize) override;
    int Output(uint8_t *buffer, int maxSize) override;

private:
    struct Packet
    {
        uint32_t sequence;
        std::vector<uint8_t> data;
    };

    // Sequence management
    uint32_t _localSequence = 0;
    uint32_t _remoteSequence = 0;
    uint32_t _conv = 0;

    // Send window (for retransmission)
    static const size_t SEND_WINDOW_SIZE = 32;
    std::array<Packet, SEND_WINDOW_SIZE> _sendWindow;

    // Receive buffer
    std::deque<Packet> _receiveQueue;
    static const size_t RECEIVE_BUFFER_SIZE = 64;

    // Timing
    uint32_t _lastUpdate = 0;
    static const uint32_t UPDATE_INTERVAL = 10; // 10ms
};

} // namespace System
