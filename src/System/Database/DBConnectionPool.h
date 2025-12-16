#pragma once

#include "System/Database/IDatabaseConnection.h"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>


namespace System {

class DBConnectionPool
{
public:
    // Factory method to create specific connection types
    using ConnectionFactory = std::function<IDatabaseConnection *()>;

    DBConnectionPool(size_t poolSize, const std::string &connString, ConnectionFactory factory);
    ~DBConnectionPool();

    void Init();

    // Get a connection. Blocks if empty? Or returns nullptr?
    // Let's implement blocking with timeout or simple non-blocking for now.
    // User plan said "Acquire".
    IDatabaseConnection *Acquire();

    // Return connection to pool
    void Release(IDatabaseConnection *conn);

private:
    size_t _poolSize;
    std::string _connectionString;
    ConnectionFactory _factory;

    std::vector<IDatabaseConnection *> _allConnections; // For cleanup
    std::queue<IDatabaseConnection *> _freeConnections;

    std::mutex _mutex;
    // std::condition_variable _cv; // If we want blocking acquire
};

} // namespace System
