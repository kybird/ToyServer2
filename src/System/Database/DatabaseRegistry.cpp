#include "System/Database/DatabaseRegistry.h"
#include "System/IDatabase.h"
#ifdef USE_SQLITE
#include "System/Drivers/SQLite/SQLiteDriver.h"
#endif
#ifdef USE_MYSQL
#include "System/Drivers/MySQL/MySQLDriver.h"
#endif
#include <mutex>

namespace System::Database {

Registry &Registry::Instance()
{
    static Registry instance;
    return instance;
}

void Registry::Initialize()
{
    if (_initialized)
        return;

#ifdef USE_SQLITE
    Register(
        "sqlite",
        [](const auto &ctx)
        {
            return CreateSQLite(ctx);
        }
    );
#endif

#ifdef USE_MYSQL
    Register(
        "mysql",
        [](const auto &ctx)
        {
            return CreateMySQL(ctx);
        }
    );
#endif

    _initialized = true;
}

void Registry::Register(const std::string &type, DatabaseFactory factory)
{
    _drivers[type] = std::move(factory);
}

std::shared_ptr<IDatabase> Registry::Create(const std::string &type, const DatabaseContext &ctx)
{
    auto it = _drivers.find(type);
    if (it != _drivers.end())
    {
        return it->second(ctx);
    }
    return nullptr;
}

std::vector<std::string> Registry::GetRegisteredDrivers()
{
    std::vector<std::string> drivers;
    for (const auto &[name, _] : _drivers)
    {
        drivers.push_back(name);
    }
    return drivers;
}

} // namespace System::Database
