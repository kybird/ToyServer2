#pragma once

#include <memory>
#include <string>

namespace System {

class IDispatcher;
class ITimer;
class IPacketHandler;

class IFramework
{
public:
    virtual ~IFramework() = default;

    // Static Factory Method
    static std::unique_ptr<IFramework> Create();

    // Core Lifecycle
    virtual bool Init(const std::string &configPath, std::shared_ptr<IPacketHandler> packetHandler) = 0;
    virtual void Run() = 0;
    virtual void Stop() = 0;

    // Accessors
    virtual std::shared_ptr<IDispatcher> GetDispatcher() const = 0;
    virtual std::shared_ptr<ITimer> GetTimer() const = 0;
};

} // namespace System
