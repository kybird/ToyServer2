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
            // Setup Async Logger
            spdlog::init_thread_pool(8192, 1);

            // Use simple stdout sink (No Color for stability)
            auto console_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/server.log", true);

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

            auto logger = std::make_shared<spdlog::async_logger>(
                "server", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block
            );

            spdlog::register_logger(logger);
            spdlog::set_default_logger(logger);
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

            SetLogLevel(level);

            // Setup File-Only Logger
            _fileLogger = std::make_shared<spdlog::async_logger>(
                "file_only", file_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::block
            );
            spdlog::register_logger(_fileLogger);
            _fileLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [FILE] %v");

            spdlog::flush_on(spdlog::level::err);
            spdlog::flush_every(std::chrono::seconds(1));
            spdlog::info("Logger Initialized (Level: {})", level);
            logger->flush();
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

    void Info(const std::string &msg) override
    {
        spdlog::info(msg);
    }

    void Warn(const std::string &msg) override
    {
        spdlog::warn(msg);
    }

    void Error(const std::string &msg) override
    {
        spdlog::error(msg);
    }

    void Debug(const std::string &msg) override
    {
        spdlog::debug(msg);
    }

    void File(const std::string &msg) override
    {
        if (_fileLogger)
        {
            _fileLogger->info(msg);
        }
    }

private:
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

    std::shared_ptr<spdlog::logger> _fileLogger;
};

ILog &GetLog()
{
    static LogImpl instance;
    return instance;
}

} // namespace System
