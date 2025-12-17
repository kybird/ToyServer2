#pragma once

#include <cstdint>
#include <functional> // Added
#include <memory>


namespace System {

class Session;

// Public Message Types
enum class MessageType {
    NETWORK_CONNECT = 0,
    NETWORK_DISCONNECT,
    NETWORK_DATA,
    LOGIC_JOB,
    LAMBDA_JOB, // Added for generic tasks

    // User defined messages start here or after reserved range
    PACKET = 10
};

// [New Architecture: Single Allocation Polymorphism]
struct IMessage
{
    virtual ~IMessage() = default;
    uint32_t type; // Changed from enum class to support internal/external extension
    uint64_t sessionId;
    Session *session = nullptr;
};

struct EventMessage : public IMessage
{
    EventMessage()
    {
        type = (uint32_t)MessageType::LOGIC_JOB;
    }
    // Generic event data can be added here if needed
};

// Generic Job for Dispatcher
struct LambdaMessage : public IMessage
{
    LambdaMessage()
    {
        type = (uint32_t)MessageType::LAMBDA_JOB;
    }
    std::function<void()> task;
};

struct PacketMessage : public IMessage
{
    PacketMessage()
    {
        type = (uint32_t)MessageType::PACKET;
    }
    uint16_t length;
    uint8_t data[1]; // Flexible Array Member

    uint8_t *Payload()
    {
        return data;
    }
    const uint8_t *Payload() const
    {
        return data;
    }

    static size_t CalculateAllocSize(uint16_t bodySize)
    {
        return sizeof(PacketMessage) + bodySize;
    }
};
} // namespace System
