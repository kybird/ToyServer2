#pragma once

#include "System/Session/Session.h"
#include <memory>
#include <string>

namespace System {

struct IPacketEncryption;
struct GatewaySessionImpl;

/**
 * @brief GatewaySession - Specialized session for external clients.
 * Inherits from Session base class.
 * Uses PIMPL to hide Boost Asio dependencies.
 */
class GatewaySession : public Session
{
    friend struct GatewaySessionImpl;

public:
    GatewaySession();
    virtual ~GatewaySession() override;

    // Pool Hooks (Overrides)
    void Reset() override;
    void Reset(std::shared_ptr<void> socket, uint64_t sessionId, IDispatcher *dispatcher);
    void OnRecycle() override;

    // ISession Interface Overrides
    void Close() override;
    void OnConnect() override;
    void OnDisconnect() override;

    // Specialized Methods
    void SetEncryption(std::unique_ptr<IPacketEncryption> encryption);
    void ConfigHeartbeat(uint32_t intervalMs, uint32_t timeoutMs, std::function<void(GatewaySession *)> pingFunc);
    void OnPong() override;

    std::string GetRemoteAddress() const;
    void OnError(const std::string &errorMsg);

protected:
    // Implementation of the virtual flush hook
    void Flush() override;

private:
    std::unique_ptr<GatewaySessionImpl> _impl;
};

} // namespace System
