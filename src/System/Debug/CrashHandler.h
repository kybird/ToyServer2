#pragma once

#include "System/Pch.h"
#include <DbgHelp.h>
#include <windows.h>


namespace System {
namespace Debug {

class CrashHandler {
public:
  static void Init();

private:
  static LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS *exceptionInfo);
};

} // namespace Debug
} // namespace System
