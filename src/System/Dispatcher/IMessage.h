#pragma once

#include "System/Network/Packet.h"
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <cstdint>


namespace System {

class ISession;

enum class MessageType { NETWORK_CONNECT, NETWORK_DISCONNECT, NETWORK_DATA, LOGIC_JOB };

struct SystemMessage
{
    MessageType type;
    uint64_t sessionId;                // 0 if system
    boost::intrusive_ptr<Packet> data; // Payload (Zero Allocation)
    ISession *session;                 // Direct Raw Pointer (Zero-Alloc, Dispatcher-Lifetime Disconnect)
};

} // namespace System
