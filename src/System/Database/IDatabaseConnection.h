#pragma once

#include <memory>
#include <string>
#include <vector>


namespace System {

// Abstract Result Set Interface
class IResultSet
{
public:
    virtual ~IResultSet() = default;

    // Move next. Returns true if row exists.
    virtual bool Next() = 0;

    // Get value by column index (0-based)
    virtual int GetInt(int columnIndex) = 0;
    virtual std::string GetString(int columnIndex) = 0;
    virtual double GetDouble(int columnIndex) = 0;

    // Get value by column name (Optional, derived classes can implement using map)
    virtual int GetInt(const std::string &columnName) = 0;
    virtual std::string GetString(const std::string &columnName) = 0;
};

class IDatabaseConnection
{
public:
    virtual ~IDatabaseConnection() = default;

    virtual bool Connect(const std::string &connectionString) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() = 0;

    // Basic KeepAlive check
    virtual bool Ping() = 0;

    // SQL Execution
    // Returns true on success
    virtual bool Execute(const std::string &sql) = 0;

    // SQL Query
    // Returns shared_ptr to ResultSet. Null on error or empty?
    // Usually returns valid object but empty results. Null on execution error.
    virtual std::shared_ptr<IResultSet> Query(const std::string &sql) = 0;
};

} // namespace System
