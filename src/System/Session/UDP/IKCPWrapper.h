#pragma once

#include <cstddef>
#include <cstdint>

namespace System {

/**
 * @brief Minimal interface for ARQ (Automatic Repeat reQuest) implementations.
 * Allows swapping between different reliable UDP implementations.
 */
class IKCPWrapper
{
public:
    virtual ~IKCPWrapper() = default;

    /**
     * @brief Initialize the ARQ protocol.
     * @param conv Conversation ID
     */
    virtual void Initialize(uint32_t conv) = 0;

    /**
     * @brief Send reliable data.
     * @param data Data to send
     * @param length Data length
     * @return Number of bytes queued
     */
    virtual int Send(const void *data, int length) = 0;

    /**
     * @brief Process received packet.
     * @param data Packet data
     * @param length Packet length
     * @return Number of bytes processed
     */
    virtual int Input(const void *data, int length) = 0;

    /**
     * @brief Update protocol state.
     * @param current Current time in milliseconds
     */
    virtual void Update(uint32_t current) = 0;

    /**
     * @brief Get received data.
     * @param buffer Output buffer
     * @param maxSize Buffer size
     * @return Number of bytes received
     */
    virtual int Recv(uint8_t *buffer, int maxSize) = 0;

    /**
     * @brief Get data to send over UDP.
     * @param buffer Output buffer
     * @param maxSize Buffer size
     * @return Number of bytes to send
     */
    virtual int Output(uint8_t *buffer, int maxSize) = 0;
};

} // namespace System
