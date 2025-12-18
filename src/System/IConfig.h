#pragma once

#include <memory>
#include <string>

namespace System {

struct ServerConfig
{
    int port = 0;
    int workerThreadCount = 0;
    int taskWorkerCount = 0;
    std::string dbAddress;

    // Rate Limiter
    double rateLimit = 50.0;
    double rateBurst = 100.0;

    // Encryption (none, xor, aes)
    std::string encryption = "none";
    std::string encryptionKey = "";
    std::string encryptionIV = "";
};

class IConfig
{
public:
    virtual ~IConfig() = default;

    virtual bool Load(const std::string &filePath) = 0;
    virtual const ServerConfig &GetConfig() const = 0;

    // Static Factory Method
    static std::shared_ptr<IConfig> Create();
};

} // namespace System
