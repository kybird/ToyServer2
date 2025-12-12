#include "System/Config/ConfigLoader.h"
#include "System/ILog.h"
#include "System/Pch.h"
#include <fstream>


namespace System {

ConfigLoader &ConfigLoader::Instance() {
  static ConfigLoader instance;
  return instance;
}

bool ConfigLoader::Load(const std::string &filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      LOG_ERROR("Failed to open config file: {}", filePath);
      return false;
    }

    nlohmann::json j;
    file >> j;

    // Load values (using safe access or default)
    if (j.contains("server")) {
      auto &server = j["server"];
      _config.port = server.value("port", 9000);
      _config.workerThreadCount =
          server.value("worker_threads", std::thread::hardware_concurrency());
      _config.dbInfo = server.value("db_info", "");
    }

    LOG_INFO("Config loaded. Port: {}, Threads: {}", _config.port,
             _config.workerThreadCount);
    return true;
  } catch (const std::exception &e) {
    LOG_ERROR("Exception loading config: {}", e.what());
    return false;
  }
}

const ServerConfig &ConfigLoader::GetConfig() const { return _config; }

} // namespace System
