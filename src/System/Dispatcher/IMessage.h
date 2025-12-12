#pragma once

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

namespace System {

class ISession;

enum class MessageType { NETWORK_CONNECT, NETWORK_DISCONNECT, NETWORK_DATA, LOGIC_JOB };

struct SystemMessage {
    MessageType type;
    uint64_t sessionId;                         // 0 if system
    std::shared_ptr<std::vector<uint8_t>> data; // Payload
    std::shared_ptr<ISession> session;          // Direct Pointer for Zero-Lookup
};

} // namespace System
