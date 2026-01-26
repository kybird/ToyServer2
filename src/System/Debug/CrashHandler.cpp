#include "System/Debug/CrashHandler.h"
#include "System/ILog.h"
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

#pragma comment(lib, "Dbghelp.lib")

namespace System {
namespace Debug {

void CrashHandler::Init()
{
    SetUnhandledExceptionFilter(ExceptionFilter);

    // [New] Hook CRT Assertions (Debug Assertion Failed)
    _CrtSetReportHook(CustomReportHook);

    LOG_INFO("CrashHandler Initialized. Minidumps enabled (Exception + CRT Assert).");
}

int CrashHandler::CustomReportHook(int reportType, char *message, int *returnValue)
{
    // Filter only Assertions
    if (reportType == _CRT_ASSERT)
    {
        LOG_ERROR("CRT ASSERTION FAILED: {}", message ? message : "Unknown");

        // Force crash/dump by raising exception or calling Filter directly?
        // Calling Filter directly with nullptr info might be limited.
        // Better to explicitly trigger a breakpoint or write dump manually.

        // Let's manually write dump here.
        std::filesystem::path dumpDir = "dumps";
        if (!std::filesystem::exists(dumpDir))
            std::filesystem::create_directory(dumpDir);

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << dumpDir.string() << "/Assert_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".dmp";

        HANDLE hFile = CreateFileA(
            ss.str().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
        );
        if (hFile != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION mdei;
            mdei.ThreadId = GetCurrentThreadId();
            mdei.ExceptionPointers = nullptr; // No exception pointers for Assert
            mdei.ClientPointers = FALSE;

            MiniDumpWriteDump(
                GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, nullptr, nullptr, nullptr
            );
            CloseHandle(hFile);
            LOG_INFO("Saved Assertion Dump to {}", ss.str());
        }
    }

    // Return 0 to let CRT display the dialog as well (or 1 to suppress?)
    // User wants to see the dialog usually.
    return 0;
}

LONG WINAPI CrashHandler::ExceptionFilter(EXCEPTION_POINTERS *exceptionInfo)
{
    // Ensure dumps directory exists
    std::filesystem::path dumpDir = "dumps";
    if (!std::filesystem::exists(dumpDir))
    {
        std::filesystem::create_directory(dumpDir);
    }

    // Generate Filename with Timestamp
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << dumpDir.string() << "/Crash_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".dmp";
    std::string dumpFile = ss.str();

    HANDLE hFile = CreateFileA(
        dumpFile.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    );

    if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE))
    {
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = exceptionInfo;
        mdei.ClientPointers = FALSE;

        MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)(
        MiniDumpWithIndirectlyReferencedMemory | // Stack pointer targets
        MiniDumpWithDataSegs |                   // Globals
        MiniDumpWithHandleData |                 // Handle info
        MiniDumpScanMemory                       // Stack memory scan
    );

        BOOL result = MiniDumpWriteDump(
            GetCurrentProcess(), GetCurrentProcessId(), hFile, mdt, (exceptionInfo != 0) ? &mdei : 0, 0, 0
        );

        CloseHandle(hFile);

        if (result)
        {
            std::cerr << "CRASH DETECTED! Minidump saved to: " << dumpFile << std::endl;
        }
        else
        {
            std::cerr << "CRASH DETECTED! Failed to save Minidump." << std::endl;
        }
    }
    else
    {
        std::cerr << "CRASH DETECTED! Failed to create dump file." << std::endl;
    }

    return EXCEPTION_EXECUTE_HANDLER; // Terminate process
}

} // namespace Debug
} // namespace System
