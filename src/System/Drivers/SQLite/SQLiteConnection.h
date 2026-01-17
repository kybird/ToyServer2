#pragma once

#include "System/Database/IConnection.h"
#include <memory>
#include <mutex>
#include <sqlite3.h>

namespace System {

class SQLiteConnection;

/**
 * SQLite 전용 결과 셋
 */
class SQLiteResultSet : public IResultSet
{
public:
    SQLiteResultSet(IConnection *conn, sqlite3_stmt *stmt, bool hasFirstRow, bool isEof, bool ownsStmt = true);
    ~SQLiteResultSet() override;

    bool Next() override;
    int GetInt(int idx) override;
    std::string GetString(int idx) override;
    double GetDouble(int idx) override;

private:
    IConnection *_conn; // 커넥션 수명 보장 (외부 Wrapper가 담당)
    sqlite3_stmt *_stmt;
    bool _ownsStmt;
    bool _firstRowPending;
    bool _isEof;
};

/**
 * SQLite 전용 Prepared Statement
 */
class SQLitePreparedStatement : public IPreparedStatement
{
public:
    SQLitePreparedStatement(IConnection *conn, sqlite3_stmt *stmt);
    ~SQLitePreparedStatement() override;

    DbStatus BindInt(int idx, int val) override;
    DbStatus BindString(int idx, const std::string &val) override;
    DbStatus BindDouble(int idx, double val) override;

    DbResult<std::unique_ptr<IResultSet>> ExecuteQuery() override;
    DbStatus ExecuteUpdate() override;

private:
    IConnection *_conn; // 커넥션 수명 보장 (외부 Wrapper가 담당)
    sqlite3_stmt *_stmt;
};

/**
 * SQLite 전용 트랜잭션
 */
class SQLiteTransaction : public ITransaction
{
public:
    SQLiteTransaction(std::shared_ptr<IConnection> conn);
    ~SQLiteTransaction() override;

    DbStatus Commit() override;

private:
    std::shared_ptr<IConnection> _conn;
    bool _committed = false;
};

/**
 * SQLite 커넥션 드라이버
 */
class SQLiteConnection : public IConnection, public std::enable_shared_from_this<SQLiteConnection>
{
public:
    SQLiteConnection();
    ~SQLiteConnection() override;

    bool Connect(const std::string &connStr) override;
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

    // SQLite 전용 접근자 (내부용)
    sqlite3 *GetRawHandle()
    {
        return _db;
    }

private:
    sqlite3 *_db = nullptr;
    bool _isConnected = false;
    bool _inTransaction = false;
    std::mutex _mutex; // 단일 스레드용이지만 안전을 위해 유지 (풀링 계층에서 이미 보장함)
};

} // namespace System
