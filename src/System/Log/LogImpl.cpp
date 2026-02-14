#include "System/ILog.h"
#include "System/Pch.h"
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace System {

class LogImpl : public ILog
{
public:
    void Init(const std::string &level) override
    {
        try
        {
            // Setup Async Logger (Global Thread Pool)
            // 1,048,576 (2^20) 크기의 큐, 1개의 백그라운드 I/O 스레드
            spdlog::init_thread_pool(1048576, 1);

            // Use simple stdout sink (No Color for stability)
            auto console_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/server.log", true);

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

            // overrun_oldest: 큐가 가득 차면 오래된 로그를 버리고 서버 틱 안정을 우선함
            _logger = std::make_shared<spdlog::async_logger>(
                "server",
                sinks.begin(),
                sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::overrun_oldest
            );

            spdlog::register_logger(_logger);
            spdlog::set_default_logger(_logger);

            // 패턴 최적화 (단순화된 포맷)
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

            SetLogLevel(level);

            // Backtrace: 크리티컬 에러 발생 시 직전 32개 로그를 쏟아냄
            _logger->enable_backtrace(32);

            spdlog::flush_on(spdlog::level::err);
            // 너무 잦은 플러시는 I/O 병목을 유발하므로 완화 (OS 버퍼 활용)
            spdlog::flush_every(std::chrono::seconds(5));

            LOG_INFO("Logger Initialized (Level: {}, Async Queue: 1048576, Policy: overrun_oldest)", level);
        } catch (const std::exception &e)
        {
            std::cerr << "Logger Init Failed: " << e.what() << std::endl;
        } catch (...)
        {
            std::cerr << "Logger Init Failed: Unknown Error" << std::endl;
        }
    }

    void SetLogLevel(const std::string &level) override
    {
        spdlog::set_level(GetSpdLevel(level));
    }

    bool ShouldLog(Log::Level level) override
    {
        return _logger && _logger->should_log(ToSpdLevel(level));
    }

    void Write(Log::Level level, std::string_view message) override
    {
        if (_logger)
        {
            _logger->log(ToSpdLevel(level), message);

            // Critical 레벨 발생 시 백트레이스 덤프 (선택 사항)
            if (level == Log::Level::Critical)
            {
                _logger->dump_backtrace();
            }
        }
    }

private:
    spdlog::level::level_enum ToSpdLevel(Log::Level level)
    {
        switch (level)
        {
        case Log::Level::Trace:
            return spdlog::level::trace;
        case Log::Level::Debug:
            return spdlog::level::debug;
        case Log::Level::Info:
            return spdlog::level::info;
        case Log::Level::Warn:
            return spdlog::level::warn;
        case Log::Level::Error:
            return spdlog::level::err;
        case Log::Level::Critical:
            return spdlog::level::critical;
        case Log::Level::Off:
            return spdlog::level::off;
        default:
            return spdlog::level::info;
        }
    }

    spdlog::level::level_enum GetSpdLevel(const std::string &level)
    {
        if (level == "trace")
            return spdlog::level::trace;
        if (level == "debug")
            return spdlog::level::debug;
        if (level == "info")
            return spdlog::level::info;
        if (level == "warn")
            return spdlog::level::warn;
        if (level == "err")
            return spdlog::level::err;
        if (level == "critical")
            return spdlog::level::critical;
        if (level == "off")
            return spdlog::level::off;
        return spdlog::level::info;
    }

    std::shared_ptr<spdlog::async_logger> _logger;
};

ILog &GetLog()
{
    static LogImpl instance;
    return instance;
}

} // namespace System
