#pragma once

#include <cstdint>
#include <functional>

namespace System {

/**
 * @brief Interface for KCP (Reliable UDP) adapter.
 * Provides C++ abstraction around libkcp C library.
 */
class IKCPAdapter
{
public:
    virtual ~IKCPAdapter() = default;

    /**
     * @brief Set callback for KCP output (sending raw packets).
     */
    virtual void SetOutputCallback(std::function<int(const char *, int)> callback) = 0;

    /**
     * @brief Send data through KCP protocol.
     * @param data Data to send
     * @param length Data length
     * @return Number of bytes queued
     */
    virtual int Send(const void *data, int length) = 0;

    /**
     * @brief Input received data from network.
     * @param data Received data
     * @param length Data length
     * @return Number of bytes processed
     */
    virtual int Input(const void *data, int length) = 0;

    /**
     * @brief Update KCP state.
     * @param current Current time in milliseconds
     */
    virtual void Update(uint32_t current) = 0;

    /**
     * @brief Get data ready to send over UDP.
     * @param buffer Output buffer
     * @param maxSize Buffer size
     * @return Number of bytes available to send
     */
    virtual int Output(uint8_t *buffer, int maxSize) = 0;

    /**
     * @brief Get received data from KCP.
     * @param buffer Output buffer
     * @param maxSize Buffer size
     * @return Number of bytes received
     */
    virtual int Recv(uint8_t *buffer, int maxSize) = 0;
};

} // namespace System
