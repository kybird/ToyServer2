#pragma once

#include <fmt/format.h> // Ensure consistency with Pch.h
#include <string_view>

namespace System {

namespace Log {
enum class Level { Trace, Debug, Info, Warn, Error, Critical, Off };
}

class ILog
{
public:
    virtual ~ILog() = default;
    virtual void Init(const std::string &level = "info") = 0;
    virtual void SetLogLevel(const std::string &level) = 0;

    // [Virtual] 구현체가 처리할 Low-Level 함수 (이미 포맷팅된 문자열만 받음)
    virtual bool ShouldLog(Log::Level level) = 0;
    virtual void Write(Log::Level level, std::string_view message) = 0;

    // [Template] 고성능 포맷팅 엔진 (Zero-Allocation)
    template <typename... Args> void LogFormat(Log::Level level, fmt::format_string<Args...> fmt, Args &&...args)
    {
        // 1KB 스택 버퍼 (힙 할당 없이 복사 비용 최소화)
        char buffer[1024];

        // 포맷팅 수행 (1024바이트 초과 시 안전하게 잘림)
        auto result = fmt::format_to_n(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);

        // 완성된 문자열 뷰를 가상 함수로 전달
        Write(level, std::string_view(buffer, result.size));
    }
};

// Global Accessor
ILog &GetLog();

} // namespace System

// Macros for easy access with Lazy Evaluation
#define LOG_TRACE(...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::System::GetLog().ShouldLog(::System::Log::Level::Trace))                                                 \
        {                                                                                                              \
            ::System::GetLog().LogFormat(::System::Log::Level::Trace, __VA_ARGS__);                                    \
        }                                                                                                              \
    } while (0)

#define LOG_DEBUG(...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::System::GetLog().ShouldLog(::System::Log::Level::Debug))                                                 \
        {                                                                                                              \
            ::System::GetLog().LogFormat(::System::Log::Level::Debug, __VA_ARGS__);                                    \
        }                                                                                                              \
    } while (0)

#define LOG_INFO(...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::System::GetLog().ShouldLog(::System::Log::Level::Info))                                                  \
        {                                                                                                              \
            ::System::GetLog().LogFormat(::System::Log::Level::Info, __VA_ARGS__);                                     \
        }                                                                                                              \
    } while (0)

#define LOG_WARN(...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::System::GetLog().ShouldLog(::System::Log::Level::Warn))                                                  \
        {                                                                                                              \
            ::System::GetLog().LogFormat(::System::Log::Level::Warn, __VA_ARGS__);                                     \
        }                                                                                                              \
    } while (0)

#define LOG_ERROR(...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::System::GetLog().ShouldLog(::System::Log::Level::Error))                                                 \
        {                                                                                                              \
            ::System::GetLog().LogFormat(::System::Log::Level::Error, __VA_ARGS__);                                    \
        }                                                                                                              \
    } while (0)

#define LOG_CRITICAL(...)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::System::GetLog().ShouldLog(::System::Log::Level::Critical))                                              \
        {                                                                                                              \
            ::System::GetLog().LogFormat(::System::Log::Level::Critical, __VA_ARGS__);                                 \
        }                                                                                                              \
    } while (0)
