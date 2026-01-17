#include "System/Drivers/SQLite/SQLiteConnection.h"
#include "System/ILog.h"

namespace System {

// --------------------------------------------------------------------------
// Helper Factory
// --------------------------------------------------------------------------
IConnection *CreateSQLiteConnection()
{
    return new SQLiteConnection();
}

// --------------------------------------------------------------------------
// SQLiteResultSet
// --------------------------------------------------------------------------
SQLiteResultSet::SQLiteResultSet(IConnection *conn, sqlite3_stmt *stmt, bool hasFirstRow, bool isEof, bool ownsStmt)
    : _conn(conn), _stmt(stmt), _firstRowPending(hasFirstRow), _isEof(isEof), _ownsStmt(ownsStmt)
{
}

SQLiteResultSet::~SQLiteResultSet()
{
    if (_stmt)
    {
        if (_ownsStmt)
        {
            sqlite3_finalize(_stmt);
        }
        else
        {
            sqlite3_reset(_stmt); // Prepared Statement 재사용을 위해 리셋
        }
    }
}

bool SQLiteResultSet::Next()
{
    if (_firstRowPending)
    {
        _firstRowPending = false;
        return true;
    }

    if (_isEof)
    {
        return false;
    }

    int rc = sqlite3_step(_stmt);
    if (rc == SQLITE_ROW)
    {
        return true;
    }

    _isEof = true;
    return false;
}

int SQLiteResultSet::GetInt(int idx)
{
    return sqlite3_column_int(_stmt, idx);
}

std::string SQLiteResultSet::GetString(int idx)
{
    const unsigned char *text = sqlite3_column_text(_stmt, idx);
    return text ? reinterpret_cast<const char *>(text) : "";
}

double SQLiteResultSet::GetDouble(int idx)
{
    return sqlite3_column_double(_stmt, idx);
}

// --------------------------------------------------------------------------
// SQLitePreparedStatement
// --------------------------------------------------------------------------
SQLitePreparedStatement::SQLitePreparedStatement(IConnection *conn, sqlite3_stmt *stmt) : _conn(conn), _stmt(stmt)
{
}

SQLitePreparedStatement::~SQLitePreparedStatement()
{
    if (_stmt)
        sqlite3_finalize(_stmt);
}

DbStatus SQLitePreparedStatement::BindInt(int idx, int val)
{
    if (sqlite3_bind_int(_stmt, idx + 1, val) != SQLITE_OK)
        return DbStatus::Error("Failed to bind int");
    return DbStatus::Ok();
}

DbStatus SQLitePreparedStatement::BindString(int idx, const std::string &val)
{
    if (sqlite3_bind_text(_stmt, idx + 1, val.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
        return DbStatus::Error("Failed to bind string");
    return DbStatus::Ok();
}

DbStatus SQLitePreparedStatement::BindDouble(int idx, double val)
{
    if (sqlite3_bind_double(_stmt, idx + 1, val) != SQLITE_OK)
        return DbStatus::Error("Failed to bind double");
    return DbStatus::Ok();
}

DbResult<std::unique_ptr<IResultSet>> SQLitePreparedStatement::ExecuteQuery()
{
    // SQLite Adapter Pattern:
    // RDBMS처럼 동작하도록 첫 번째 행을 Prefetch하여 쿼리 오류를 조기에 감지함.

    // 1. Reset (재사용을 위해)
    sqlite3_reset(_stmt);

    // 2. Prefetch
    int rc = sqlite3_step(_stmt);

    bool hasRow = false;
    bool isEof = false;

    if (rc == SQLITE_ROW)
    {
        hasRow = true;
    }
    else if (rc == SQLITE_DONE)
    {
        isEof = true;
    }
    else
    {
        // 실제 실행 에러 감지!
        return DbResult<std::unique_ptr<IResultSet>>::Fail(
            DbStatusCode::DB_INVALID_QUERY, sqlite3_errmsg(sqlite3_db_handle(_stmt))
        );
    }

    // 3. ResultSet 생성 (ownsStmt = false -> Prepared Statement가 소유권 유지, 다만 리셋은 RS가 소멸시 할지 결정해야
    // 함) 원래 코드에서는 PreparedStatement가 소유권을 공유하는 구조였으나, 여기서는 Query 결과가 나오면 상태 관리
    // 책임이 넘어가는 구조. ownsStmt=false로 전달하지만, 내부 로직상 RS가 리셋을 책임져야 할 수도 있음. 기존 코드:
    // ownsStmt = false for PreparedStatement child.

    return DbResult<std::unique_ptr<IResultSet>>::Success(
        std::make_unique<SQLiteResultSet>(_conn, _stmt, hasRow, isEof, false)
    );
}

DbStatus SQLitePreparedStatement::ExecuteUpdate()
{
    int rc = sqlite3_step(_stmt);
    sqlite3_reset(_stmt); // For next reuse
    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
        return DbStatus::Error("Update failed");
    return DbStatus::Ok();
}

// --------------------------------------------------------------------------
// SQLiteTransaction
// --------------------------------------------------------------------------
SQLiteTransaction::SQLiteTransaction(std::shared_ptr<IConnection> conn) : _conn(conn)
{
}

SQLiteTransaction::~SQLiteTransaction()
{
    if (!_committed)
    {
        _conn->Rollback();
    }
}

DbStatus SQLiteTransaction::Commit()
{
    DbStatus status = _conn->Commit();
    if (status.IsOk())
        _committed = true;
    return status;
}

// --------------------------------------------------------------------------
// SQLiteConnection
// --------------------------------------------------------------------------
SQLiteConnection::SQLiteConnection()
{
}

SQLiteConnection::~SQLiteConnection()
{
    Disconnect();
}

bool SQLiteConnection::Connect(const std::string &connStr)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (sqlite3_open_v2(
            connStr.c_str(), &_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr
        ) != SQLITE_OK)
    {
        LOG_ERROR("SQLite Connection failed: {}", sqlite3_errmsg(_db));
        return false;
    }

    // 서버 환경 최적화 설정 (WAL, busy_timeout)
    sqlite3_busy_timeout(_db, 5000); // 5초 대기
    sqlite3_exec(_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    _isConnected = true;
    return true;
}

void SQLiteConnection::Disconnect()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_db)
    {
        sqlite3_close(_db);
        _db = nullptr;
    }
    _isConnected = false;
}

bool SQLiteConnection::IsConnected()
{
    return _isConnected;
}

bool SQLiteConnection::Ping()
{
    return _isConnected; // SQLite는 로컬 파일이므로 연결 상태 확인으로 충분
}

DbStatus SQLiteConnection::Execute(const std::string &sql)
{
    std::lock_guard<std::mutex> lock(_mutex);
    char *zErrMsg = nullptr;
    if (sqlite3_exec(_db, sql.c_str(), nullptr, 0, &zErrMsg) != SQLITE_OK)
    {
        std::string msg = zErrMsg;
        sqlite3_free(zErrMsg);
        return DbStatus::Error(msg);
    }
    return DbStatus::Ok();
}

DbResult<std::unique_ptr<IResultSet>> SQLiteConnection::Query(const std::string &sql)
{
    std::lock_guard<std::mutex> lock(_mutex);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_INVALID_QUERY, sqlite3_errmsg(_db));
    }
    // 단순 Query도 일관성을 위해 Prefetch 로직을 사용하는 것이 좋으나,
    // 여기서는 직접 로직을 구성합니다.
    int rc = sqlite3_step(stmt);
    bool hasRow = (rc == SQLITE_ROW);
    bool isEof = (rc == SQLITE_DONE);

    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_INVALID_QUERY, sqlite3_errmsg(_db));
    }

    // 만약 첫 스텝이 DONE이면 바로 EOF 상태.
    // 만약 첫 스텝이 ROW이면 Pending 상태.
    // Query()는 stmt 생성 -> 실행 -> 결과 반환이므로 ownsStmt = true
    return DbResult<std::unique_ptr<IResultSet>>::Success(
        std::make_unique<SQLiteResultSet>(this, stmt, hasRow, isEof, true)
    );
}

DbResult<std::unique_ptr<IPreparedStatement>> SQLiteConnection::Prepare(const std::string &sql)
{
    std::lock_guard<std::mutex> lock(_mutex);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        return DbResult<std::unique_ptr<IPreparedStatement>>::Fail(DbStatusCode::DB_INVALID_QUERY, sqlite3_errmsg(_db));
    }
    return DbResult<std::unique_ptr<IPreparedStatement>>::Success(
        std::make_unique<SQLitePreparedStatement>(this, stmt)
    );
}

DbStatus SQLiteConnection::BeginTransaction()
{
    if (_inTransaction)
        return {DbStatusCode::DB_TRANSACTION_ACTIVE, "Transaction already active"};
    DbStatus status = Execute("BEGIN TRANSACTION;");
    if (status.IsOk())
        _inTransaction = true;
    return status;
}

DbStatus SQLiteConnection::Commit()
{
    if (!_inTransaction)
        return DbStatus::Error("No active transaction");
    DbStatus status = Execute("COMMIT;");
    if (status.IsOk())
        _inTransaction = false;
    return status;
}

DbStatus SQLiteConnection::Rollback()
{
    if (!_inTransaction)
        return DbStatus::Error("No active transaction");
    DbStatus status = Execute("ROLLBACK;");
    _inTransaction = false; // 롤백 시도 후에는 무조건 해제
    return status;
}

void SQLiteConnection::ResetState()
{
    if (_inTransaction)
    {
        LOG_DEBUG("SQLite: Active transaction found on release. Rolling back.");
        Rollback();
    }
    // 추가적인 세션 초기화가 필요한 경우 여기서 수행
}

} // namespace System
