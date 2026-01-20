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

    // Timer System Messages
    LOGIC_TIMER,
    LOGIC_TIMER_EXPIRED,
    LOGIC_TIMER_ADD,
    LOGIC_TIMER_CANCEL,
    LOGIC_TIMER_TICK,

    // User defined messages start here or after reserved range
    PACKET = 10
};

// [New Architecture: Single Allocation Polymorphism]
struct IMessage
{
    virtual ~IMessage() = default;

    // [Reference Counting]
    // Initialized to 1.
    mutable std::atomic<int> refCount{1};

    void AddRef() const
    {
        refCount.fetch_add(1, std::memory_order_relaxed);
    }

    // Returns true if refCount reached 0
    bool DecRef() const
    {
        return refCount.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

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

    // [Reference Counting Integration]
    // Use IMessage::AddRef() and MessagePool::Free() for lifecycle.
    // We can't implement Release here easily without including MessagePool.h (Circular)
    // Or we make MessagePool::Free handle it?
    // "Release" usually implies "DecRef and Free if 0".
    // Let's call it DecRefAndCheck? Or just expose DecRef?

    // Better: Make MessagePool::Free check refCount.
    // But MessagePool::Free takes IMessage*. PacketMessage is a subclass.
    // IMessage needs virtual RefCount?
    // Coding Convention 4.2 warns against virtuals in HotPath?
    // But IMessage has virtual destructor already.

    // Optimized: Only PacketMessage needs refcounting?
    // Events are unicast.
    // If we put refCount in IMessage base, it's safer.
    // Let's put it in IMessage base to simplify MessagePool::Free.

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
