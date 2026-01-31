#include "System/Drivers/SQLite/SQLiteDriver.h"
#include "System/Database/DatabaseImpl.h"
#include "System/Drivers/SQLite/SQLiteConnectionFactory.h"


namespace System::Database {

std::shared_ptr<IDatabase> CreateSQLite(const DatabaseContext &ctx)
{
    auto factory = std::make_unique<System::SQLiteConnectionFactory>();

    // DatabaseImpl을 사용하는 SQLite 드라이버 인스턴스 생성
    return std::make_shared<System::DatabaseImpl>(
        ctx.config.dbAddress,
        ctx.config.dbWorkerCount,
        5000, // default timeout
        std::move(factory),
        ctx.dbThreadPool,
        ctx.dispatcher
    );
}

} // namespace System::Database
