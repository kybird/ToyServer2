#include "System/Framework/Framework.h"
#include "System/Config/ConfigLoader.h"
#include "System/Debug/CrashHandler.h"
#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h" // Added
#include "System/ILog.h"
#include "System/ITimer.h"           // Fixed: ITimer.h contains SetGlobalTimer decl
#include "System/Monitor/IMonitor.h" // Fixed: IMonitor.h
#include "System/Network/NetworkImpl.h"
#include "System/Thread/ThreadPool.h"
#include "System/Timer/TimerImpl.h"
#include <iostream>

#include "System/Dispatcher/MessagePool.h" // Added
#include "System/Session/Session.h"        // Added
#include "System/Session/SessionFactory.h" // Added
#include "System/Session/SessionPool.h"

namespace System {

std::unique_ptr<IFramework> IFramework::Create()
{
    return std::make_unique<Framework>();
}

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

    // 2. Prepare Pools (Hidden from User)
    LOG_INFO("Pre-allocating MessagePool & SessionPool...");
    System::MessagePool::Prepare(6000);                     // Configurable?
    System::SessionPool<System::Session>::Init(1000, 1000); // Configurable?
    LOG_INFO("Pools Ready.");

    // 3. Components
    auto dispatcherImpl = std::make_shared<DispatcherImpl>(packetHandler);
    _dispatcher = dispatcherImpl;
    // SetGlobalDispatcher(_dispatcher); // Removed

    _network = std::make_shared<NetworkImpl>();
    _network->SetDispatcher(_dispatcher.get()); // Inject Dispatcher (Raw Pointer)
    _timer = std::make_shared<TimerImpl>(_network->GetIOContext(), _dispatcher.get());

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
        // Optimization: Handle all available packets before yielding
        if (_dispatcher->Process())
        {
            // Packet processed, don't yield, check queue again immediately
            continue;
        }
        else
        {
            // Queue empty, yield to OS
            std::this_thread::yield();
        }
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
