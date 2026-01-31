#pragma once

#include "System/IConfig.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace System {

class IDatabase;
class ThreadPool;
class IDispatcher;

namespace Database {

/**
 * 데이터베이스 인스턴스 생성을 위한 컨텍스트
 */
struct DatabaseContext
{
    const ServerConfig &config;
    std::shared_ptr<ThreadPool> dbThreadPool;
    std::shared_ptr<IDispatcher> dispatcher;
};

using DatabaseFactory = std::function<std::shared_ptr<IDatabase>(const DatabaseContext &)>;

/**
 * 데이터베이스 드라이버 레지스트리 (공개 인터페이스)
 */
class Registry
{
public:
    static Registry &Instance();
    void Initialize();

    // Legacy support wrappers or direct usage
    void Register(const std::string &type, DatabaseFactory factory);
    std::shared_ptr<IDatabase> Create(const std::string &type, const DatabaseContext &ctx);
    std::vector<std::string> GetRegisteredDrivers();

private:
    Registry() = default;
    std::map<std::string, DatabaseFactory> _drivers;
    bool _initialized = false;
};

} // namespace Database
} // namespace System
