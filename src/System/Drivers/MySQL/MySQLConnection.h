#pragma once

#include "System/Database/IConnection.h"
#include <memory>
#include <mutex>
#include <string>

// Forward declaration for MySQL C API
struct MYSQL;

namespace System {

/**
 * MySQL 연결 설정
 */
struct MySQLConfig
{
    std::string Host = "localhost";
    int Port = 3306;
    std::string User;
    std::string Password;
    std::string Database;
};

/**
 * MySQL 커넥션 드라이버 (libmysqlclient C API 기반)
 */
class MySQLConnection : public IConnection
{
public:
    MySQLConnection(const MySQLConfig &config);
    ~MySQLConnection() override;

    bool Connect(const std::string &connStr) override; // connStr는 무시, config 사용
    void Disconnect() override;
    bool IsConnected() override;
    bool Ping() override;

    DbStatus Execute(const std::string &sql) override;
    DbResult<std::unique_ptr<IResultSet>> Query(const std::string &sql) override;
    DbResult<std::unique_ptr<IPreparedStatement>> Prepare(const std::string &sql) override;

    DbStatus BeginTransaction() override;
    DbStatus Commit() override;
    DbStatus Rollback() override;

    void ResetState() override;

    bool SupportsPreparedStatements() const override
    {
        return true;
    }
    bool SupportsTransactions() const override
    {
        return true;
    }

    // 내부용 접근자
    MYSQL *GetRawHandle()
    {
        return _mysql;
    }

private:
    MySQLConfig _config;
    MYSQL *_mysql = nullptr;
    bool _isConnected = false;
    bool _inTransaction = false;
    std::mutex _mutex;
};

} // namespace System
