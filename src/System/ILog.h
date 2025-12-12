#pragma once

#include <format>
#include <iostream>
#include <memory>
#include <string>

namespace System {

class ILog {
public:
    virtual ~ILog() = default;
    virtual void Init() = 0;
    virtual void Info(const std::string &msg) = 0;
    virtual void Error(const std::string &msg) = 0;
    virtual void Debug(const std::string &msg) = 0;
    virtual void File(const std::string &msg) = 0;
};

// Global Accessor
ILog &GetLog();

} // namespace System

// Macros for easy access
#define LOG_INFO(fmt, ...) System::GetLog().Info(std::format(fmt, __VA_ARGS__))
#define LOG_ERROR(fmt, ...) System::GetLog().Error(std::format(fmt, __VA_ARGS__))
#define LOG_DEBUG(fmt, ...) System::GetLog().Debug(std::format(fmt, __VA_ARGS__))
#define LOG_FILE(fmt, ...) System::GetLog().File(std::format(fmt, __VA_ARGS__))

// For simple strings without args
#define LOG_INFO_S(msg) System::GetLog().Info(msg)
#define LOG_ERROR_S(msg) System::GetLog().Error(msg)
#define LOG_FILE_S(msg) System::GetLog().File(msg)
