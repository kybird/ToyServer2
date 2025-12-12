#include "ServerPacketHandler.h"
#include "System/Debug/CrashHandler.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Framework/Framework.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include "System/Timer/ITimer.h" // Functionality: Timer
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

#include "System/Memory/ObjectPool.h"
#include "System/Memory/PacketPool.h"
#include "System/Network/ASIO/AsioSession.h"
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

        {
            // 2. Create Packet Handler (User Logic)
            auto packetHandler = std::make_shared<ServerPacketHandler>();

            // 3. Create & Run Framework
            // Stack allocated, finding config in "server_config.json"
            System::Framework framework;

            // Signal Handling
            g_SignalHandler = [&](int signal)
            {
                LOG_INFO("Signal {} received. Stopping framework...", signal);
                framework.Stop();
            };
            std::signal(SIGINT, SignalHandlerWrapper);
            std::signal(SIGTERM, SignalHandlerWrapper);

            if (framework.Init("server_config.json", packetHandler))
            {

#ifdef ENABLE_MEMORY_PROFILE
                // Memory Monitor (1 sec interval)
                framework.GetTimer()->SetInterval(
                    std::chrono::seconds(1),
                    [&framework]()
                    {
                        auto active = System::Debug::MemoryMetrics::GetActiveAllocations();
                        auto packetPoolSize = System::PacketPool::GetPoolSize();
                        auto sessionPoolSize = System::ObjectPool<System::AsioSession>::GetPoolSize();
                        auto queueSize = framework.GetDispatcher()->GetQueueSize();

                        LOG_INFO(
                            "Mem: Alloc={}, PktPool={}, SessPool={}, Queue={}",
                            active,
                            packetPoolSize,
                            sessionPoolSize,
                            queueSize
                        );
                    }
                );
#endif

                framework.Run();

                // 4. Memory Cleanup Verification (Must be done while Framework/IOContext is alive)
                LOG_INFO("Starting Memory Cleanup...");
                System::ObjectPool<System::AsioSession>::Clear();
                System::PacketPool::Clear();
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
