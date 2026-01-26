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
#include "System/Session/BackendSession.h"
#include "System/Session/GatewaySession.h"
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

// PImpl Definition for Signal Handling
struct Framework::SignalImpl
{
    boost::asio::signal_set signals;
    SignalImpl(boost::asio::io_context &ctx) : signals(ctx, SIGINT, SIGTERM)
    {
    }
};

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

    // 1.5 Server Role Setup
    if (serverConfig.serverRole == "backend")
    {
        SessionFactory::SetServerRole(ServerRole::Backend);
        System::SessionPool<System::BackendSession>::Init(1000, 1000);
    }
    else
    {
        SessionFactory::SetServerRole(ServerRole::Gateway);
        System::SessionPool<System::GatewaySession>::Init(1000, 1000);
    }

    // 2. Prepare Pools & Encryption (Hidden from User)
    LOG_INFO("Pre-allocating MessagePool...");
    System::MessagePool::Prepare(6000);

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
    LOG_ERROR("[DEBUG] Framework calling Network->Start({})", port);
    if (!_network->Start(port))
    {
        LOG_ERROR("[DEBUG] Network->Start returned FALSE!");
        LOG_ERROR("Failed to start network on port {}", port);
        return false;
    }
    LOG_ERROR("[DEBUG] Network->Start returned TRUE!");

    // 6. Console
    _console = std::make_shared<CommandConsole>(_config);

    // 7. Internal Signal Handling (Graceful Shutdown)
    _signals = std::make_unique<SignalImpl>(_network->GetIOContext());
    _signals->signals.async_wait(
        [this](const boost::system::error_code &error, int signal_number)
        {
            if (!error)
            {
                LOG_INFO("Signal {} received. Stopping framework internally...", signal_number);
                Stop();
            }
        }
    );

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
                    LOG_ERROR("[DEBUG] IO Thread #{} Started.", i);
                    _network->Run();
                    LOG_ERROR("[DEBUG] IO Thread #{} Stopped.", i);
                } catch (const std::exception &e)
                {
                    // If we are shutting down, ignore resource deadlock errors from logger/asio
                    if (_running)
                    {
                        LOG_ERROR("IO Thread #{} Exception: {}", i, e.what());
                    }
                }
            }
        );
    }

    // 2. Start Task Pool
    _threadPool->Start();
    if (_dbThreadPool)
        _dbThreadPool->Start();

    // 3. Main Thread Logic Loop
    auto lastLog = std::chrono::steady_clock::now();
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

        // [DEBUG] Heartbeat
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastLog).count() >= 5)
        {
            LOG_ERROR("[DEBUG] Server Main Loop Alive...");
            lastLog = now;
        }
    }
}

void Framework::Stop()
{
    // [Fix] Signal Handler Safety
    // This function can be called from Signal Handler (IO Thread).
    // DO NOT Join IO threads here, or it causes self-join deadlock/crash.
    // _ioThreads are std::jthread, so they will join automatically in ~Framework().

    if (!_running)
        return; // Already stopped

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
        _network->Stop(); // Stops IO Context, IO threads will exit loop soon

    // Do NOT join or clear _ioThreads here.
}

void Framework::Join()
{
    LOG_INFO("Joining Threads and Cleaning up...");

    if (_console)
        _console->Stop();

    // 1. Shutdown MQ
    System::MQ::MessageSystem::Instance().Shutdown();

    // 2. Stop Thread Pools (if not already)
    if (_threadPool)
        _threadPool->Stop();
    if (_dbThreadPool)
        _dbThreadPool->Stop();

    // 3. Wait for IO Threads
    // Since we use std::jthread, they would join in destructor anyway,
    // but explicit join allows controlled order and logging.
    for (auto &t : _ioThreads)
    {
        if (t.joinable())
        {
            try
            {
                t.join();
            } catch (const std::exception &e)
            {
                LOG_ERROR("Error joining IO thread: {}", e.what());
            }
        }
    }
    _ioThreads.clear();

    LOG_INFO("Shutdown Complete.");
}

} // namespace System
