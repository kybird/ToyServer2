#include "System/Config/Json/JsonConfigLoader.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include <fstream>
#include <thread>

namespace System {

bool JsonConfigLoader::Load(const std::string &filePath)
{
    try
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open config file: {}", filePath);
            return false;
        }

        nlohmann::json j;
        file >> j;

        // Load values (using safe access or default)
        if (j.contains("server"))
        {
            auto &server = j["server"];
            _config.port = server.value("port", 9000);
            _config.workerThreadCount = server.value("worker_threads", (int)std::thread::hardware_concurrency());
            _config.taskWorkerCount = server.value("task_worker_threads", (int)std::thread::hardware_concurrency());
            _config.dbAddress = server.value("db_info", "");
            _config.rateLimit = server.value("rate_limit", 50.0);
            _config.rateBurst = server.value("rate_burst", 100.0);
        }

        LOG_INFO(
            "Config loaded. Port: {}, IO Threads: {}, Task Threads: {}",
            _config.port,
            _config.workerThreadCount,
            _config.taskWorkerCount
        );
        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("Exception loading config: {}", e.what());
        return false;
    }
}

const ServerConfig &JsonConfigLoader::GetConfig() const
{
    return _config;
}

} // namespace System
