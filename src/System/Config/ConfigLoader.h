#pragma once

#include <nlohmann/json.hpp>
#include <string>


namespace System {

struct ServerConfig {
  int port = 0;
  int workerThreadCount = 0;
  std::string dbInfo;
};

class ConfigLoader {
public:
  // Singleton Access
  static ConfigLoader &Instance();

  bool Load(const std::string &filePath);
  const ServerConfig &GetConfig() const;

private:
  ServerConfig _config;
};

} // namespace System
