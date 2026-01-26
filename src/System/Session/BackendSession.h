#pragma once

#include "System/Session/Session.h"
#include <memory>
#include <string>

namespace System {

struct BackendSessionImpl;

/**
 * @brief BackendSession - Zero-copy session for internal server communication.
 * Inherits from Session base class.
 * Uses PIMPL to hide Boost Asio.
 */
class BackendSession : public Session
{
    friend struct BackendSessionImpl;

public:
    BackendSession();
    virtual ~BackendSession() override;

    // Pool Hooks
    void Reset() override;
    void Reset(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher);
    void OnRecycle() override;

    // ISession Overrides
    void Close() override;
    void OnConnect() override;
    void OnDisconnect() override;

    // Specialized Methods
    void ConfigHeartbeat(uint32_t intervalMs, uint32_t timeoutMs, std::function<void(BackendSession *)> pingFunc);
    void OnPong() override;

    std::string GetRemoteAddress() const;
    void OnError(const std::string &errorMsg);

protected:
    void Flush() override;

private:
    std::unique_ptr<BackendSessionImpl> _impl;
};

} // namespace System
