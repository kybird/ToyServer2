#include "System/IFramework.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace System {

class NetworkImpl;
class ITimer;
class IDispatcher;
class IMetrics; // Forward Decl
class ThreadPool;
class IPacketHandler;
class IPacketHandler;
class CommandConsole;  // Forward Decl
class ICommandConsole; // Forward Decl
class IDatabase;       // Forward Decl

class Framework : public IFramework
{
public:
    Framework();
    ~Framework();

    // Dependency Injection: User provides PacketHandler
    bool Init(std::shared_ptr<IConfig> config, std::shared_ptr<IPacketHandler> packetHandler) override;
    void Run() override;
    void Stop() override;

    std::shared_ptr<ITimer> GetTimer() const override;
    std::shared_ptr<IStrand> CreateStrand() override;
    size_t GetDispatcherQueueSize() const override
    {
        return _dispatcher ? _dispatcher->GetQueueSize() : 0;
    }

protected:
    std::shared_ptr<IDispatcher> GetDispatcher() const override;
    std::shared_ptr<ICommandConsole> GetCommandConsole() const override;

private:
    std::shared_ptr<NetworkImpl> _network;
    std::shared_ptr<ITimer> _timer;
    std::shared_ptr<IDispatcher> _dispatcher;
    std::shared_ptr<ThreadPool> _threadPool;
    std::shared_ptr<ThreadPool> _dbThreadPool;
    std::vector<std::jthread> _ioThreads;
    std::atomic<bool> _running{false};

    std::shared_ptr<CommandConsole> _console;
    std::shared_ptr<IConfig> _config;
};

} // namespace System
