#include "ServerPacketHandler.h"
#include "System/Debug/CrashHandler.h"
#include "System/Framework/Framework.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include <iostream>
#include <string>

#define ENABLE_MEMORY_PROFILE
#ifdef ENABLE_MEMORY_PROFILE
#include "System/Debug/MemoryMetrics.h"

void *operator new(size_t size) {
    System::Debug::MemoryMetrics::AllocCount++;
    return malloc(size);
}

void operator delete(void *ptr) noexcept {
    System::Debug::MemoryMetrics::DeallocCount++;
    free(ptr);
}
#endif

int main(int argc, char *argv[]) {
    try {
        System::Debug::CrashHandler::Init();

        // Check for Crash Test
        if (argc > 1 && std::string(argv[1]) == "--crash-test") {
            std::cout << "Running Crash Test... BOOM!" << std::endl;
            int *crashPtr = nullptr;
            *crashPtr = 42; // Access Violation
        }

        // 1. Init Logger
        System::GetLog().Init();

        // 2. Create Packet Handler (User Logic)
        auto packetHandler = std::make_shared<ServerPacketHandler>();

        // 3. Create & Run Framework
        // Stack allocated, finding config in "server_config.json"
        System::Framework framework;
        if (framework.Init("server_config.json", packetHandler)) {
            framework.Run();
        } else {
            LOG_ERROR("Framework Initialization Failed.");
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "FATAL ERROR in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FATAL ERROR in main: Unknown Exception" << std::endl;
        return 1;
    }
    return 0;
}
