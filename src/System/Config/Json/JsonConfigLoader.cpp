#include "System/Config/Json/JsonConfigLoader.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include <fstream>
#include <thread>

namespace System {

std::shared_ptr<IConfig> IConfig::Create()
{
    return std::make_shared<JsonConfigLoader>();
}

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

            // Robust thread count loading (Support both underscore and camelCase)
            if (server.contains("worker_threads"))
                _config.workerThreadCount = server["worker_threads"].get<int>();
            else if (server.contains("workerThreadCount"))
                _config.workerThreadCount = server["workerThreadCount"].get<int>();
            else
                _config.workerThreadCount = (int)std::thread::hardware_concurrency();

            if (server.contains("task_worker_threads"))
                _config.taskWorkerCount = server["task_worker_threads"].get<int>();
            else if (server.contains("taskWorkerCount"))
                _config.taskWorkerCount = server["taskWorkerCount"].get<int>();
            else
                _config.taskWorkerCount = (int)std::thread::hardware_concurrency();

            _config.dbAddress = server.value("db_info", server.value("dbAddress", ""));
            _config.rateLimit = server.value("rate_limit", server.value("rateLimit", 50.0));
            _config.rateBurst = server.value("rate_burst", server.value("rateBurst", 100.0));
            _config.encryption = server.value("encryption", "none");
            _config.encryptionKey = server.value("encryption_key", server.value("encryptionKey", ""));
            _config.encryptionIV = server.value("encryption_iv", server.value("encryptionIV", ""));
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
