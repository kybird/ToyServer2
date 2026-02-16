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
    // string_view로 전달, spdlog가 내부적으로 복사/이동 처리
    virtual void Write(Log::Level level, std::string_view message) noexcept = 0;

    // [Template] 고성능 포맷팅 엔진 (Zero-Allocation for small messages)
    template <typename... Args> void LogFormat(Log::Level level, fmt::format_string<Args...> fmt_str, Args &&...args)
    {
        // [CRITICAL] 로그 레벨 체크를 먼저 수행 (불필요한 포맷팅 연산 방지)
        if (!ShouldLog(level))
            return;

        // 1. fmt::format으로 직접 std::string 생성 (SSO: 64~128바이트까지 힙 할당 없음)
        std::string message = fmt::format(fmt_str, std::forward<Args>(args)...);

        // 2. std::move로 소유권 이전 → 큐가 밀려도 안전 (message가 주인)
        Write(level, std::move(message));
    }

    // 기존 인터페이스 호환성 유지
    virtual void Info(const std::string &msg) = 0;
    virtual void Warn(const std::string &msg) = 0;
    virtual void Error(const std::string &msg) = 0;
    virtual void Debug(const std::string &msg) = 0;
    virtual void File(const std::string &msg) = 0;
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

// LOG_FILE uses Info level (for backward compatibility)
#define LOG_FILE(...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::System::GetLog().ShouldLog(::System::Log::Level::Info))                                                  \
        {                                                                                                              \
            ::System::GetLog().LogFormat(::System::Log::Level::Info, __VA_ARGS__);                                     \
        }                                                                                                              \
    } while (0)
