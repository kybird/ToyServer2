#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>


namespace System {

class ISession {
public:
    virtual ~ISession() = default;

    // Zero-copy send (shared_ptr)
    virtual void Send(std::shared_ptr<std::vector<uint8_t>> packet) = 0;

    // Send helper (might copy if not careful, but useful for small packets)
    virtual void Send(std::span<const uint8_t> data) = 0;

    virtual void Close() = 0;
    virtual uint64_t GetId() const = 0;
};

} // namespace System
