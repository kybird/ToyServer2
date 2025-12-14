#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace System {

struct PacketMessage;

class ISession
{
public:
    virtual ~ISession() = default;

    // Zero-copy send (PacketMessage from MessagePool)
    virtual void Send(PacketMessage *msg) = 0;

    // Send helper (allocates from MessagePool internally)
    virtual void Send(std::span<const uint8_t> data) = 0;

    virtual void Close() = 0;
    virtual uint64_t GetId() const = 0;
    virtual void Reset() = 0;
    virtual bool CanDestroy() const = 0;
    virtual void OnRecycle()
    {
    }

    // [Lifetime] Reference counting for async message queue safety
    virtual void DecRef() = 0;
};

} // namespace System
