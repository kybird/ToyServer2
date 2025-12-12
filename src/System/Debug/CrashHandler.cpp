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

void CrashHandler::Init() {
  SetUnhandledExceptionFilter(ExceptionFilter);
  LOG_INFO("CrashHandler Initialized. Minidumps enabled.");
}

LONG WINAPI CrashHandler::ExceptionFilter(EXCEPTION_POINTERS *exceptionInfo) {
  // Ensure dumps directory exists
  std::filesystem::path dumpDir = "dumps";
  if (!std::filesystem::exists(dumpDir)) {
    std::filesystem::create_directory(dumpDir);
  }

  // Generate Filename with Timestamp
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << dumpDir.string() << "/Crash_"
     << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".dmp";
  std::string dumpFile = ss.str();

  HANDLE hFile = CreateFileA(dumpFile.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                             NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE)) {
    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId = GetCurrentThreadId();
    mdei.ExceptionPointers = exceptionInfo;
    mdei.ClientPointers = FALSE;

    MINIDUMP_TYPE mdt =
        MiniDumpNormal; // Basic dump. Use MiniDumpWithFullMemory for full heap
                        // (large file).

    BOOL result =
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          mdt, (exceptionInfo != 0) ? &mdei : 0, 0, 0);

    CloseHandle(hFile);

    if (result) {
      std::cerr << "CRASH DETECTED! Minidump saved to: " << dumpFile
                << std::endl;
    } else {
      std::cerr << "CRASH DETECTED! Failed to save Minidump." << std::endl;
    }
  } else {
    std::cerr << "CRASH DETECTED! Failed to create dump file." << std::endl;
  }

  return EXCEPTION_EXECUTE_HANDLER; // Terminate process
}

} // namespace Debug
} // namespace System
