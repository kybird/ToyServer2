#pragma once

#include "IDatabaseConnection.h"
#include "System/ILog.h"
#include <mutex>
#include <sqlite3.h>


namespace System {

class SQLiteResultSet : public IResultSet
{
public:
    SQLiteResultSet(sqlite3_stmt *stmt) : _stmt(stmt)
    {
    }
    ~SQLiteResultSet()
    {
        if (_stmt)
            sqlite3_finalize(_stmt);
    }

    bool Next() override
    {
        if (!_stmt)
            return false;
        int rc = sqlite3_step(_stmt);
        return rc == SQLITE_ROW;
    }

    int GetInt(int columnIndex) override
    {
        return sqlite3_column_int(_stmt, columnIndex);
    }

    std::string GetString(int columnIndex) override
    {
        const unsigned char *text = sqlite3_column_text(_stmt, columnIndex);
        return text ? std::string(reinterpret_cast<const char *>(text)) : "";
    }

    double GetDouble(int columnIndex) override
    {
        return sqlite3_column_double(_stmt, columnIndex);
    }

    // Optional: Implement mapping if needed, for now throw or ignore
    int GetInt(const std::string &columnName) override
    {
        return 0;
    }
    std::string GetString(const std::string &columnName) override
    {
        return "";
    }

private:
    sqlite3_stmt *_stmt = nullptr;
};

class SQLiteConnection : public IDatabaseConnection
{
public:
    SQLiteConnection() = default;
    ~SQLiteConnection() override
    {
        Disconnect();
    }

    bool Connect(const std::string &connectionString) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        int rc = sqlite3_open(connectionString.c_str(), &_db);
        if (rc)
        {
            LOG_ERROR("Can't open database: {}", sqlite3_errmsg(_db));
            return false;
        }
        _connected = true;
        LOG_INFO("Opened database successfully: {}", connectionString);
        return true;
    }

    void Disconnect() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_db)
        {
            sqlite3_close(_db);
            _db = nullptr;
        }
        _connected = false;
    }

    bool IsConnected() override
    {
        return _connected;
    }

    bool Ping() override
    {
        return IsConnected();
    }

    bool Execute(const std::string &sql) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_db)
            return false;

        char *zErrMsg = 0;
        int rc = sqlite3_exec(_db, sql.c_str(), nullptr, 0, &zErrMsg);
        if (rc != SQLITE_OK)
        {
            LOG_ERROR("SQL error: {}", zErrMsg);
            sqlite3_free(zErrMsg);
            return false;
        }
        return true;
    }

    std::shared_ptr<IResultSet> Query(const std::string &sql) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_db)
            return nullptr;

        sqlite3_stmt *stmt;
        // prepare_v2 allows multiple statements? No, verify.
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            LOG_ERROR("SQL prepare error: {}", sqlite3_errmsg(_db));
            return nullptr;
        }

        return std::make_shared<SQLiteResultSet>(stmt);
    }

private:
    sqlite3 *_db = nullptr;
    bool _connected = false;
    std::mutex _mutex;
};

} // namespace System
