#include "System/Drivers/MySQL/MySQLConnection.h"
#include "System/ILog.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <mysql/mysql.h>
#include <vector>

namespace System {

// ===========================================================================
// MySQLResultSet - mysql_store_result / mysql_fetch_row 기반
// ===========================================================================
class MySQLResultSet : public IResultSet
{
public:
    MySQLResultSet(MYSQL_RES *res) : _res(res), _currentRow(nullptr), _lengths(nullptr), _numFields(0)
    {
        if (_res)
        {
            _numFields = mysql_num_fields(_res);
        }
    }

    ~MySQLResultSet() override
    {
        if (_res)
        {
            mysql_free_result(_res);
        }
    }

    bool Next() override
    {
        if (!_res)
            return false;
        _currentRow = mysql_fetch_row(_res);
        _lengths = mysql_fetch_lengths(_res);
        return _currentRow != nullptr;
    }

    int GetInt(int idx) override
    {
        if (!_currentRow || idx < 0 || idx >= static_cast<int>(_numFields))
            return 0;
        return _currentRow[idx] ? std::atoi(_currentRow[idx]) : 0;
    }

    std::string GetString(int idx) override
    {
        if (!_currentRow || idx < 0 || idx >= static_cast<int>(_numFields))
            return "";
        if (!_currentRow[idx])
            return "";
        return std::string(_currentRow[idx], _lengths ? _lengths[idx] : std::strlen(_currentRow[idx]));
    }

    double GetDouble(int idx) override
    {
        if (!_currentRow || idx < 0 || idx >= static_cast<int>(_numFields))
            return 0.0;
        return _currentRow[idx] ? std::atof(_currentRow[idx]) : 0.0;
    }

private:
    MYSQL_RES *_res;
    MYSQL_ROW _currentRow;
    unsigned long *_lengths;
    unsigned int _numFields;
};

// ===========================================================================
// MySQLPreparedStatement - mysql_stmt_* 기반
// ===========================================================================
class MySQLPreparedStatement : public IPreparedStatement
{
public:
    MySQLPreparedStatement(MYSQL *mysql, MYSQL_STMT *stmt, int paramCount)
        : _mysql(mysql), _stmt(stmt), _paramCount(paramCount)
    {
        if (_paramCount > 0)
        {
            _binds.resize(_paramCount);
            _intValues.resize(_paramCount);
            _doubleValues.resize(_paramCount);
            _stringValues.resize(_paramCount);
            _stringLengths.resize(_paramCount);
            std::memset(_binds.data(), 0, sizeof(MYSQL_BIND) * _paramCount);
        }
    }

    ~MySQLPreparedStatement() override
    {
        if (_stmt)
        {
            mysql_stmt_close(_stmt);
        }
    }

    DbStatus BindInt(int idx, int val) override
    {
        if (idx < 0 || idx >= _paramCount)
            return DbStatus::Error("Invalid parameter index");

        _intValues[idx] = val;
        _binds[idx].buffer_type = MYSQL_TYPE_LONG;
        _binds[idx].buffer = &_intValues[idx];
        _binds[idx].is_null = nullptr;
        _binds[idx].length = nullptr;
        return DbStatus::Ok();
    }

    DbStatus BindString(int idx, const std::string &val) override
    {
        if (idx < 0 || idx >= _paramCount)
            return DbStatus::Error("Invalid parameter index");

        _stringValues[idx] = val;
        _stringLengths[idx] = static_cast<unsigned long>(_stringValues[idx].size());
        _binds[idx].buffer_type = MYSQL_TYPE_STRING;
        _binds[idx].buffer = const_cast<char *>(_stringValues[idx].c_str());
        _binds[idx].buffer_length = static_cast<unsigned long>(_stringValues[idx].size());
        _binds[idx].is_null = nullptr;
        _binds[idx].length = &_stringLengths[idx];
        return DbStatus::Ok();
    }

    DbStatus BindDouble(int idx, double val) override
    {
        if (idx < 0 || idx >= _paramCount)
            return DbStatus::Error("Invalid parameter index");

        _doubleValues[idx] = val;
        _binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
        _binds[idx].buffer = &_doubleValues[idx];
        _binds[idx].is_null = nullptr;
        _binds[idx].length = nullptr;
        return DbStatus::Ok();
    }

    DbResult<std::unique_ptr<IResultSet>> ExecuteQuery() override
    {
        if (_paramCount > 0)
        {
            if (mysql_stmt_bind_param(_stmt, _binds.data()) != 0)
            {
                return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_ERROR, mysql_stmt_error(_stmt));
            }
        }

        if (mysql_stmt_execute(_stmt) != 0)
        {
            return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_ERROR, mysql_stmt_error(_stmt));
        }

        // PreparedStatement로 SELECT 쿼리 결과를 받아오려면
        // mysql_stmt_result_metadata, mysql_stmt_bind_result, mysql_stmt_fetch 등이 필요함
        // 간단 구현을 위해 mysql_store_result 대신 mysql_stmt_store_result 사용

        if (mysql_stmt_store_result(_stmt) != 0)
        {
            return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_ERROR, mysql_stmt_error(_stmt));
        }

        // NOTE: Prepared Statement 결과는 mysql_stmt_fetch로 가져와야 하므로
        // 별도의 MySQLStmtResultSet이 필요함. 현재는 간단히 에러 반환.
        // 실제 사용 시에는 Query() 메서드 사용을 권장.
        LOG_WARN(
            "MySQL PreparedStatement ExecuteQuery: Use Query() for SELECT statements. "
            "Prepared statement result binding is complex and not fully implemented."
        );

        return DbResult<std::unique_ptr<IResultSet>>::Fail(
            DbStatusCode::DB_ERROR,
            "PreparedStatement ExecuteQuery not fully implemented. Use Query() for SELECT statements."
        );
    }

    DbStatus ExecuteUpdate() override
    {
        if (_paramCount > 0)
        {
            if (mysql_stmt_bind_param(_stmt, _binds.data()) != 0)
            {
                return DbStatus::Error(mysql_stmt_error(_stmt));
            }
        }

        if (mysql_stmt_execute(_stmt) != 0)
        {
            return DbStatus::Error(mysql_stmt_error(_stmt));
        }

        return DbStatus::Ok();
    }

private:
    MYSQL *_mysql;
    MYSQL_STMT *_stmt;
    int _paramCount;
    std::vector<MYSQL_BIND> _binds;
    std::vector<int> _intValues;
    std::vector<double> _doubleValues;
    std::vector<std::string> _stringValues;
    std::vector<unsigned long> _stringLengths;
};

// ===========================================================================
// MySQLConnection - mysql_real_connect 기반
// ===========================================================================

MySQLConnection::MySQLConnection(const MySQLConfig &config)
    : _config(config), _mysql(nullptr), _isConnected(false), _inTransaction(false)
{
}

MySQLConnection::~MySQLConnection()
{
    Disconnect();
}

bool MySQLConnection::Connect(const std::string & /* connStr - 무시 */)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _mysql = mysql_init(nullptr);
    if (!_mysql)
    {
        LOG_ERROR("MySQL: mysql_init failed");
        return false;
    }

    // 연결
    MYSQL *result = mysql_real_connect(
        _mysql,
        _config.Host.c_str(),
        _config.User.c_str(),
        _config.Password.c_str(),
        _config.Database.c_str(),
        _config.Port,
        nullptr, // unix_socket
        0        // client_flag
    );

    if (!result)
    {
        LOG_ERROR("MySQL Connect Error: {}", mysql_error(_mysql));
        mysql_close(_mysql);
        _mysql = nullptr;
        return false;
    }

    // UTF-8 설정
    mysql_set_character_set(_mysql, "utf8mb4");

    _isConnected = true;
    return true;
}

void MySQLConnection::Disconnect()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_mysql)
    {
        mysql_close(_mysql);
        _mysql = nullptr;
    }
    _isConnected = false;
    _inTransaction = false;
}

bool MySQLConnection::IsConnected()
{
    return _isConnected && _mysql != nullptr;
}

bool MySQLConnection::Ping()
{
    if (!IsConnected())
        return false;
    std::lock_guard<std::mutex> lock(_mutex);
    return mysql_ping(_mysql) == 0;
}

DbStatus MySQLConnection::Execute(const std::string &sql)
{
    if (!IsConnected())
        return DbStatus::Error("Not connected");

    std::lock_guard<std::mutex> lock(_mutex);
    if (mysql_query(_mysql, sql.c_str()) != 0)
    {
        return DbStatus::Error(mysql_error(_mysql));
    }

    // 결과가 있으면 소비
    MYSQL_RES *res = mysql_store_result(_mysql);
    if (res)
    {
        mysql_free_result(res);
    }

    return DbStatus::Ok();
}

DbResult<std::unique_ptr<IResultSet>> MySQLConnection::Query(const std::string &sql)
{
    if (!IsConnected())
        return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_CONNECTION_FAILURE, "Not connected");

    std::lock_guard<std::mutex> lock(_mutex);

    if (mysql_query(_mysql, sql.c_str()) != 0)
    {
        return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_INVALID_QUERY, mysql_error(_mysql));
    }

    MYSQL_RES *res = mysql_store_result(_mysql);
    if (!res)
    {
        // 결과가 없는 쿼리(INSERT, UPDATE 등)이거나 에러
        if (mysql_field_count(_mysql) == 0)
        {
            // 결과가 없는 쿼리 (이건 Query가 아니라 Execute로 호출해야 함)
            return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_ERROR, "No result set for query");
        }
        return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_ERROR, mysql_error(_mysql));
    }

    return DbResult<std::unique_ptr<IResultSet>>::Success(std::make_unique<MySQLResultSet>(res));
}

DbResult<std::unique_ptr<IPreparedStatement>> MySQLConnection::Prepare(const std::string &sql)
{
    if (!IsConnected())
        return DbResult<std::unique_ptr<IPreparedStatement>>::Fail(
            DbStatusCode::DB_CONNECTION_FAILURE, "Not connected"
        );

    std::lock_guard<std::mutex> lock(_mutex);

    MYSQL_STMT *stmt = mysql_stmt_init(_mysql);
    if (!stmt)
    {
        return DbResult<std::unique_ptr<IPreparedStatement>>::Fail(DbStatusCode::DB_ERROR, "mysql_stmt_init failed");
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
    {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return DbResult<std::unique_ptr<IPreparedStatement>>::Fail(DbStatusCode::DB_INVALID_QUERY, err);
    }

    int paramCount = static_cast<int>(mysql_stmt_param_count(stmt));

    return DbResult<std::unique_ptr<IPreparedStatement>>::Success(
        std::make_unique<MySQLPreparedStatement>(_mysql, stmt, paramCount)
    );
}

DbStatus MySQLConnection::BeginTransaction()
{
    if (_inTransaction)
        return {DbStatusCode::DB_TRANSACTION_ACTIVE, "Transaction already active"};

    std::lock_guard<std::mutex> lock(_mutex);
    if (mysql_autocommit(_mysql, 0) != 0)
    {
        return DbStatus::Error(mysql_error(_mysql));
    }

    _inTransaction = true;
    return DbStatus::Ok();
}

DbStatus MySQLConnection::Commit()
{
    if (!_inTransaction)
        return DbStatus::Error("No active transaction");

    std::lock_guard<std::mutex> lock(_mutex);
    if (mysql_commit(_mysql) != 0)
    {
        return DbStatus::Error(mysql_error(_mysql));
    }

    mysql_autocommit(_mysql, 1);
    _inTransaction = false;
    return DbStatus::Ok();
}

DbStatus MySQLConnection::Rollback()
{
    if (!_inTransaction)
        return DbStatus::Error("No active transaction");

    std::lock_guard<std::mutex> lock(_mutex);
    if (mysql_rollback(_mysql) != 0)
    {
        std::string err = mysql_error(_mysql);
        _inTransaction = false;
        mysql_autocommit(_mysql, 1);
        return DbStatus::Error(err);
    }

    mysql_autocommit(_mysql, 1);
    _inTransaction = false;
    return DbStatus::Ok();
}

void MySQLConnection::ResetState()
{
    if (_inTransaction)
    {
        LOG_DEBUG("MySQL: Active transaction found on release. Rolling back.");
        Rollback();
    }
}

} // namespace System
