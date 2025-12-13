#pragma once

#include "System/Network/Packet.h"
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>


namespace System {

class ISession;

enum class MessageType { NETWORK_CONNECT, NETWORK_DISCONNECT, NETWORK_DATA, LOGIC_JOB };

struct SystemMessage
{
    MessageType type;
    uint64_t sessionId;                // 0 if system
    boost::intrusive_ptr<Packet> data; // Payload (Zero Allocation)
    std::shared_ptr<ISession> session; // Direct Pointer for Zero-Lookup
};

} // namespace System
