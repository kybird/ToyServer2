#pragma once

#include "System/IConfig.h"
#include <nlohmann/json.hpp>

namespace System {

class JsonConfigLoader : public IConfig
{
public:
    JsonConfigLoader() = default;
    ~JsonConfigLoader() override = default;

    bool Load(const std::string &filePath) override;
    const ServerConfig &GetConfig() const override;

private:
    ServerConfig _config;
};

} // namespace System
