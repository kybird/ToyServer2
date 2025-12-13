#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "System/Network/Packet.h"
#include <boost/smart_ptr/intrusive_ptr.hpp>


namespace System {

class ISession
{
public:
    virtual ~ISession() = default;

    // Zero-copy send (boost::intrusive_ptr<Packet>)
    virtual void Send(boost::intrusive_ptr<Packet> packet) = 0;

    // Send helper (might copy if not careful, but useful for small packets)
    virtual void Send(std::span<const uint8_t> data) = 0;

    virtual void Close() = 0;
    virtual uint64_t GetId() const = 0;
};

} // namespace System
