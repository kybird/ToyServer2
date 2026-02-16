#include "System/ILog.h"
#include "System/Pch.h"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
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
            // Rotating file sink: 최대 10MB, 5개 파일 유지 (Disk Full 방지)
            auto file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/server.log", 1024 * 1024 * 10, 5);

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

            // 패턴 최적화 (고성능 모드: 시간 계산 최소화)
            spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");

            SetLogLevel(level);

            // Backtrace: 크리티컬 에러 발생 시 직전 32개 로그를 쏟아냄
            _logger->enable_backtrace(32);

            // [성능과 안정성 분리] 에러 핸들러 등록 (Write 내부 try-catch 제거로 성능 향상)
            _logger->set_error_handler(
                [](const std::string &msg)
                {
                    std::cerr << "[spdlog Error] " << msg << "\n";
                }
            );

            spdlog::flush_on(spdlog::level::err);
            // 너무 잦은 플러시는 I/O 병목을 유발하므로 완화 (OS 버퍼 활용)
            spdlog::flush_every(std::chrono::seconds(5));

            // [File 전용 로거] 콘솔 출력 없이 파일에만 기록 (대량 데이터 기록용)
            auto file_only_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/file_only.log", 1024 * 1024 * 50, 10);
            _fileLogger = std::make_shared<spdlog::async_logger>(
                "file_only", file_only_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest
            );
            spdlog::register_logger(_fileLogger);
            // 최소 패턴 (CPU 점유율 최소화)
            _fileLogger->set_pattern("%v");
            _fileLogger->set_level(spdlog::level::info);

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

    void Write(Log::Level level, std::string_view message) noexcept override
    {
        if (_logger)
        {
            // try-catch 제거로 컴파일러 최적화 유도 (error_handler가 예외 처리)
            _logger->log(ToSpdLevel(level), message);

            // Critical 레벨 발생 시 백트레이스 덤프
            if (level == Log::Level::Critical)
                _logger->dump_backtrace();
        }
    }

    // 기존 인터페이스 호환성 유지
    void Info(const std::string &msg) override
    {
        Write(Log::Level::Info, msg);
    }
    void Warn(const std::string &msg) override
    {
        Write(Log::Level::Warn, msg);
    }
    void Error(const std::string &msg) override
    {
        Write(Log::Level::Error, msg);
    }
    void Debug(const std::string &msg) override
    {
        Write(Log::Level::Debug, msg);
    }
    void File(const std::string &msg) override
    {
        // [File 전용 로거 사용] 콘솔 출력 없이 파일에만 기록
        if (_fileLogger)
        {
            try
            {
                _fileLogger->info(msg);
            } catch (const std::exception &e)
            {
                std::cerr << "[FileLog Failed] " << e.what() << "\n";
            } catch (...)
            {
                std::cerr << "[FileLog Failed] Unknown error\n";
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
    std::shared_ptr<spdlog::async_logger> _fileLogger; // 파일 전용 로거 (콘솔 출력 없음)
};

ILog &GetLog()
{
    static LogImpl instance;
    return instance;
}

} // namespace System
