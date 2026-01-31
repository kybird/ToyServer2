#include "System/Drivers/MySQL/MySQLDriver.h"
#include "System/Database/DatabaseImpl.h"
#include "System/Drivers/MySQL/MySQLConnectionFactory.h"


namespace System::Database {

std::shared_ptr<IDatabase> CreateMySQL(const DatabaseContext &ctx)
{
    // dbAddress (예: "localhost:3306;user;pass;dbname")을 파싱하여 MySQLConfig 구성
    // 실제 파싱 로직은 프로젝트의 관례에 따름
    System::MySQLConfig mysqlCfg;

    // 단순 파싱 예시 (실제 구현에 맞춰 조정 필요)
    std::string addr = ctx.config.dbAddress;
    size_t pos = 0;
    auto next_token = [&]()
    {
        size_t end = addr.find(';', pos);
        std::string token = addr.substr(pos, end - pos);
        pos = (end == std::string::npos) ? std::string::npos : end + 1;
        return token;
    };

    std::string hostPort = next_token();
    size_t colon = hostPort.find(':');
    if (colon != std::string::npos)
    {
        mysqlCfg.Host = hostPort.substr(0, colon);
        mysqlCfg.Port = std::stoi(hostPort.substr(colon + 1));
    }
    else
    {
        mysqlCfg.Host = hostPort;
    }

    mysqlCfg.User = next_token();
    mysqlCfg.Password = next_token();
    mysqlCfg.Database = next_token();

    auto factory = std::make_unique<System::MySQLConnectionFactory>(mysqlCfg);

    return std::make_shared<System::DatabaseImpl>(
        ctx.config.dbAddress, ctx.config.dbWorkerCount, 5000, std::move(factory), ctx.dbThreadPool, ctx.dispatcher
    );
}

} // namespace System::Database
