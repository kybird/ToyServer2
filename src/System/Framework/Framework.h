#pragma once

#include <memory>
#include <string>

namespace System {

class AsioService;
class ITimer;
class IDispatcher;
class ThreadPool;
class IPacketHandler;

class Framework {
public:
    Framework();
    ~Framework();

    // Dependency Injection: User provides PacketHandler
    bool Init(const std::string &configPath, std::shared_ptr<IPacketHandler> packetHandler);
    void Run();

    std::shared_ptr<IDispatcher> GetDispatcher() const { return _dispatcher; }
    std::shared_ptr<ITimer> GetTimer() const { return _timer; }

private:
    std::shared_ptr<AsioService> _network;
    std::shared_ptr<ITimer> _timer;
    std::shared_ptr<IDispatcher> _dispatcher;
    std::shared_ptr<ThreadPool> _threadPool;
};

} // namespace System
