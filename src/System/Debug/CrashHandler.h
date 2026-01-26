#pragma once

#include "System/Pch.h"
#include <DbgHelp.h>
#include <windows.h>

namespace System {
namespace Debug {

class CrashHandler
{
public:
    static void Init();

private:
    static LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS *exceptionInfo);
    static int CustomReportHook(int reportType, char *message, int *returnValue);
};

} // namespace Debug
} // namespace System
