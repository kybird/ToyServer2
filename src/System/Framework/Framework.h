#include "System/IFramework.h"

namespace System {

class NetworkImpl;
class ITimer;
class IDispatcher;
class ThreadPool;
class IPacketHandler;

class Framework : public IFramework
{
public:
    Framework();
    ~Framework();

    // Dependency Injection: User provides PacketHandler
    bool Init(const std::string &configPath, std::shared_ptr<IPacketHandler> packetHandler) override;
    void Run() override;
    void Stop() override;

    std::shared_ptr<IDispatcher> GetDispatcher() const override
    {
        return _dispatcher;
    }
    std::shared_ptr<ITimer> GetTimer() const override
    {
        return _timer;
    }

private:
    std::shared_ptr<NetworkImpl> _network;
    std::shared_ptr<ITimer> _timer;
    std::shared_ptr<IDispatcher> _dispatcher;
    std::shared_ptr<ThreadPool> _threadPool;
    std::atomic<bool> _running{false};
};

} // namespace System
