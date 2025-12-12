#include "System/Framework/Framework.h"
#include "System/Config/ConfigLoader.h"
#include "System/Debug/CrashHandler.h"
#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h" // Added
#include "System/ILog.h"
#include "System/Monitor/IMonitor.h" // Fixed: IMonitor.h
#include "System/Network/ASIO/AsioService.h"
#include "System/Thread/ThreadPool.h"
#include "System/Timer/ASIO/AsioTimer.h"
#include "System/Timer/ITimer.h" // Fixed: ITimer.h contains SetGlobalTimer decl
#include <iostream>

namespace System {

Framework::Framework()
{
}

Framework::~Framework()
{
    LOG_INFO("Framework Shutting Down...");
    if (_threadPool)
        _threadPool->Stop();
}

bool Framework::Init(const std::string &configPath, std::shared_ptr<IPacketHandler> packetHandler)
{
    // 1. Config
    if (!ConfigLoader::Instance().Load(configPath))
    {
        LOG_INFO("Config loaded.");
    }

    // 3. Components
    auto dispatcherImpl = std::make_shared<DispatcherImpl>(packetHandler);
    _dispatcher = dispatcherImpl;
    // SetGlobalDispatcher(_dispatcher); // Removed

    _network = std::make_shared<AsioService>();
    _network->SetDispatcher(_dispatcher.get()); // Inject Dispatcher (Raw Pointer)
    _timer = std::make_shared<AsioTimer>(_network->GetIOContext());

    // 4. ThreadPool
    int threadCount = ConfigLoader::Instance().GetConfig().workerThreadCount;
    _threadPool = std::make_shared<ThreadPool>(threadCount);

    // 5. Init Network
    int port = ConfigLoader::Instance().GetConfig().port;
    if (!_network->Start(port))
    {
        LOG_ERROR("Failed to start network on port {}", port);
        return false;
    }

    LOG_INFO("Framework Initialized.");
    return true;
}

void Framework::Run()
{
    LOG_INFO("Framework Running...");
    _running = true;

    // 1. Start IO Threads
    _threadPool->Start(
        [this]()
        {
            try
            {
                _network->Run();
            } catch (const std::exception &e)
            {
                LOG_ERROR("IO Thread Exception: {}", e.what());
            }
        }
    );

    // 2. Main Thread Logic Loop
    while (_running)
    {
        _dispatcher->Process();
        std::this_thread::yield();
    }
}

void Framework::Stop()
{
    _running = false;
    // We can also stop thread pool here or let destructor handle it.
    // Explicit stop is good for deterministic shutdown.
    if (_threadPool)
        _threadPool->Stop();
    if (_network)
        _network->Stop();
}
} // namespace System
