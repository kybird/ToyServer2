#include "System/Framework/Framework.h"
#include "System/Console/CommandConsole.h"
#include "System/MQ/MessageSystem.h"

#include "System/Debug/CrashHandler.h"
#include "System/Dispatcher/DISPATCHER/DispatcherImpl.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/IConfig.h"
#include "System/IDatabase.h"
#include "System/ILog.h"
#include "System/ITimer.h"
#include "System/Metrics/IMetrics.h"
#include "System/Network/AesEncryption.h"
#include "System/Network/NetworkImpl.h"
#include "System/Network/XorEncryption.h"
#include "System/Session/Session.h"
#include "System/Session/SessionFactory.h"
#include "System/Session/SessionPool.h"
#include "System/Thread/Strand.h"
#include "System/Thread/ThreadPool.h"
#include "System/Timer/TimerImpl.h"
#include <algorithm> // for min
#include <vector>

#include <iostream>

namespace System {

std::unique_ptr<IFramework> IFramework::Create()
{
    return std::make_unique<Framework>();
}

std::shared_ptr<ITimer> Framework::GetTimer() const
{
    return _timer;
}

std::shared_ptr<IStrand> Framework::CreateStrand()
{
    return std::make_shared<Strand>(_threadPool);
}

std::shared_ptr<IDispatcher> Framework::GetDispatcher() const
{
    return _dispatcher;
}

std::shared_ptr<ICommandConsole> Framework::GetCommandConsole() const
{
    return _console;
}

std::shared_ptr<ThreadPool> Framework::GetThreadPool() const
{
    return _threadPool;
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

bool Framework::Init(std::shared_ptr<IConfig> config, std::shared_ptr<IPacketHandler> packetHandler)
{
    // 1. Config
    _config = config; // Store config if needed (add member to Framework.h?)
    // Actually Framwork.h needs to store it or just use it here.
    // For now, let's use it locally for initialization.
    // Wait, other components (thread pool, network) need it.
    // Framework should probably HOLD the config since it's the root.

    // Store it:
    // (Wait, Framework.h doesn't have _config member yet. I should add it first?
    // Or just use the passed pointer. The pointer is robust.

    const auto &serverConfig = _config->GetConfig();

    // 2. Prepare Pools & Encryption (Hidden from User)
    LOG_INFO("Pre-allocating MessagePool & SessionPool...");
    System::MessagePool::Prepare(6000);
    System::SessionPool<System::Session>::Init(1000, 1000);

    // [Encryption] Configure Factory
    std::string encType = serverConfig.encryption;
    if (encType == "xor")
    {
        uint8_t key = 0xA5;
        if (!serverConfig.encryptionKey.empty())
        {
            try
            {
                key = (uint8_t)std::stoi(serverConfig.encryptionKey);
            } catch (...)
            {
                key = (uint8_t)serverConfig.encryptionKey[0];
            }
        }

        SessionFactory::SetEncryptionFactory(
            [key]()
            {
                return std::make_unique<XorEncryption>(key);
            }
        );
        LOG_INFO("Encryption Enabled: XOR (Key: {})", key);
    }
    else if (encType == "aes")
    {
        std::vector<uint8_t> key(16, 0);
        std::vector<uint8_t> iv(16, 0);

        const auto &kStr = serverConfig.encryptionKey;
        std::memcpy(key.data(), kStr.data(), std::min(key.size(), kStr.size()));

        const auto &ivStr = serverConfig.encryptionIV;
        std::memcpy(iv.data(), ivStr.data(), std::min(iv.size(), ivStr.size()));

        SessionFactory::SetEncryptionFactory(
            [key, iv]()
            {
                return std::make_unique<AesEncryption>(key, iv);
            }
        );
        LOG_INFO("Encryption Enabled: AES-128-CBC");
    }
    else
    {
        LOG_INFO("Encryption: None");
    }

    // [RateLimiter] Configure from config file
    SessionFactory::SetRateLimitConfig(serverConfig.rateLimit, serverConfig.rateBurst);
    LOG_INFO("RateLimiter Config: rate={}, burst={}", serverConfig.rateLimit, serverConfig.rateBurst);

    LOG_INFO("Pools Ready.");

    // 3. Components
    auto dispatcherImpl = std::make_shared<DispatcherImpl>(packetHandler);
    _dispatcher = dispatcherImpl;

    _network = std::make_shared<NetworkImpl>();
    _network->SetDispatcher(_dispatcher.get()); // Inject Dispatcher (Raw Pointer)
    _timer = std::make_shared<TimerImpl>(_network->GetIOContext(), _dispatcher.get());

    // 4. ThreadPool (Computations)
    int taskThreads = serverConfig.taskWorkerCount;
    if (taskThreads <= 0)
    {
        LOG_ERROR("Invalid Configuration: 'task_worker_threads' must be positive. Found: {}", taskThreads);
        return false;
    }
    _threadPool = std::make_shared<ThreadPool>(taskThreads);

    // 4.5 DB ThreadPool
    int dbThreads = serverConfig.dbWorkerCount;
    if (dbThreads <= 0)
        dbThreads = 1;
    _dbThreadPool = std::make_shared<ThreadPool>(dbThreads, "DB Thread");

    // 5. Init Network
    int port = serverConfig.port;
    if (!_network->Start(port))
    {
        LOG_ERROR("Failed to start network on port {}", port);
        return false;
    }

    // 6. Console
    _console = std::make_shared<CommandConsole>(_config);

    LOG_INFO("Framework Initialized.");
    return true;
}

void Framework::Run()
{
    LOG_INFO("Framework Running...");
    _running = true;

    // 0. Start Console
    if (_console)
        _console->Start();

    // 1. Start IO Threads (Network)
    int ioThreadCount = _config->GetConfig().workerThreadCount;
    LOG_INFO("Starting {} IO Threads...", ioThreadCount);
    _ioThreads.reserve(ioThreadCount);
    for (int i = 0; i < ioThreadCount; ++i)
    {
        _ioThreads.emplace_back(
            [this, i]()
            {
                try
                {
                    _network->Run();
                } catch (const std::exception &e)
                {
                    LOG_ERROR("IO Thread #{} Exception: {}", i, e.what());
                }
            }
        );
    }

    // 2. Start Task Pool
    _threadPool->Start();
    if (_dbThreadPool)
        _dbThreadPool->Start();

    // 3. Main Thread Logic Loop
    while (_running)
    {
        // Optimization: Handle all available packets before yielding
        if (_dispatcher->Process())
        {
            continue;
        }
        else
        {
            _dispatcher->Wait(10); // Wait up to 10ms for new messages
        }
    }
}

void Framework::Stop()
{
    _running = false;

    if (_console)
        _console->Stop();

    // Shutdown MQ System before stopping ThreadPool
    // This ensures poll tasks can exit gracefully
    System::MQ::MessageSystem::Instance().Shutdown();

    if (_threadPool)
        _threadPool->Stop();
    if (_dbThreadPool)
        _dbThreadPool->Stop();
    if (_network)
        _network->Stop();

    // Join IO threads
    for (auto &t : _ioThreads)
    {
        if (t.joinable())
            t.join();
    }
    _ioThreads.clear();
}

} // namespace System
