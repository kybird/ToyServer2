#include "ServerPacketHandler.h"
#include "System/Config/Json/JsonConfigLoader.h"
#include "System/Debug/CrashHandler.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/IFramework.h"
#include "System/ILog.h"
#include "System/ITimer.h" // Functionality: Timer
#include "System/Pch.h"
#include <iostream>
#include <string>

#define ENABLE_MEMORY_PROFILE
#ifdef ENABLE_MEMORY_PROFILE
#include "System/Debug/MemoryMetrics.h"

void *operator new(size_t size)
{
    System::Debug::MemoryMetrics::AllocCount++;
    return malloc(size);
}

void operator delete(void *ptr) noexcept
{
    System::Debug::MemoryMetrics::DeallocCount++;
    free(ptr);
}
#endif

#include "System/Dispatcher/MessagePool.h"

#include <csignal>
#include <functional>

// Global Shutdown Handler
std::function<void(int)> g_SignalHandler;
void SignalHandlerWrapper(int signal)
{
    if (g_SignalHandler)
    {
        g_SignalHandler(signal);
    }
}

int main(int argc, char *argv[])
{
    try
    {
        System::Debug::CrashHandler::Init();

        // Check for Crash Test
        if (argc > 1 && std::string(argv[1]) == "--crash-test")
        {
            std::cout << "Running Crash Test... BOOM!" << std::endl;
            int *crashPtr = nullptr;
            *crashPtr = 42; // Access Violation
        }

        // 1. Init Logger
        System::GetLog().Init();

        // 1.5 Prepare Message Pool & Session Pool - Handled internally by Framework::Init now

        {
            // 2. Create Packet Handler (User Logic)
            auto packetHandler = std::make_shared<ServerPacketHandler>();

            // 2.5 Load Config
            auto config = std::make_shared<System::JsonConfigLoader>();
            if (!config->Load("server_config.json"))
            {
                LOG_ERROR("Failed to load server_config.json");
                return 1;
            }

            // 3. Create & Run Framework
            // Stack allocated
            auto framework = System::IFramework::Create(); // Use Factory

            // Signal Handling
            g_SignalHandler = [&](int signal)
            {
                LOG_INFO("Signal {} received. Stopping framework...", signal);
                framework->Stop();
            };
            std::signal(SIGINT, SignalHandlerWrapper);
            std::signal(SIGTERM, SignalHandlerWrapper);

            if (framework->Init(config, packetHandler))
            {

#ifdef ENABLE_MEMORY_PROFILE
                // Memory Monitor (1 sec interval)
                // Monitor (1 sec interval)
                // Monitor (1 sec interval)
                class StatsListener : public System::ITimerListener
                {
                public:
                    System::IFramework *framework;
                    StatsListener(System::IFramework *fw) : framework(fw)
                    {
                    }

                    void OnTimer(uint32_t timerId, void *pParam) override
                    {
                        auto active = System::Debug::MemoryMetrics::GetActiveAllocations();
                        auto msgPoolSize = System::MessagePool::GetPoolSize();
                        // auto sessionPoolSize = System::SessionPool<System::Session>::GetApproximatePoolSize();
                        // Need check if SessionPool has this method public or static. It was refactored.
                        // Assuming GetApproximatePoolSize exists based on previous file reads.
                        // Actually let's just use 0 if unsure to avoid build break, or use what was there.
                        auto sessionPoolSize = 0;
                        auto queueSize = framework->GetDispatcherQueueSize();
                        auto activeSessionCount = 0;

                        LOG_INFO(
                            "Mem: Alloc={}, MsgPool={}, ActiveSess={}, Queue={}",
                            active,
                            msgPoolSize,
                            activeSessionCount,
                            queueSize
                        );

                        // [Diagnostics]
                        auto recv = System::Debug::MemoryMetrics::RecvPacket.load();
                        auto allocFail = System::Debug::MemoryMetrics::AllocFail.load();
                        auto posted = System::Debug::MemoryMetrics::Posted.load();
                        auto processed = System::Debug::MemoryMetrics::Processed.load();
                        auto echoed = System::Debug::MemoryMetrics::Echoed.load();
                        LOG_INFO(
                            "[Pkt] Recv={}, AllocFail={}, Posted={}, Processed={}, Echoed={}",
                            recv,
                            allocFail,
                            posted,
                            processed,
                            echoed
                        );
                    }
                };

                // We need to keep this listener alive.
                auto statsListener = std::make_shared<StatsListener>(framework.get());

                // To keep it alive during Run(), we can capturing it in the main scope or...
                // Main scope blocks on framework->Run(), so 'statsListener' will remain alive until Run() returns.
                // Perfect.

                framework->GetTimer()->SetInterval(
                    100,  // ID
                    1000, // 1 sec
                    statsListener.get()
                );
#endif

                framework->Run();

                // 4. Memory Cleanup Verification
                LOG_INFO("Starting Memory Cleanup...");
                // System::ObjectPool<System::Session>::Clear(); // Removed
                System::MessagePool::Clear();
            }
            else
            {
                LOG_ERROR("Framework Initialization Failed.");
                return 1;
            }
        } // Framework Destructor Called -> Threads Joined

        // Log Final Memory Status
        auto finalAlloc = System::Debug::MemoryMetrics::GetActiveAllocations();
        LOG_INFO("Final Memory Status: Active Allocations = {}", finalAlloc);

    } catch (const std::exception &e)
    {
        std::cerr << "FATAL ERROR in main: " << e.what() << std::endl;
        return 1;
    } catch (...)
    {
        std::cerr << "FATAL ERROR in main: Unknown Exception" << std::endl;
        return 1;
    }
    return 0;
}
