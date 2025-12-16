#include "System/Database/DBConnectionPool.h"
#include "System/ILog.h"

namespace System {

DBConnectionPool::DBConnectionPool(size_t poolSize, const std::string &connString, ConnectionFactory factory)
    : _poolSize(poolSize), _connectionString(connString), _factory(factory)
{
}

DBConnectionPool::~DBConnectionPool()
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto *conn : _allConnections)
    {
        conn->Disconnect();
        delete conn;
    }
    _allConnections.clear();

    // Clear queue
    std::queue<IDatabaseConnection *> empty;
    std::swap(_freeConnections, empty);
}

void DBConnectionPool::Init()
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (size_t i = 0; i < _poolSize; ++i)
    {
        IDatabaseConnection *conn = _factory();
        if (conn->Connect(_connectionString))
        {
            _allConnections.push_back(conn);
            _freeConnections.push(conn);
        }
        else
        {
            LOG_ERROR("Failed to create/connect initial DB connection.");
            delete conn;
        }
    }
    LOG_INFO("DBConnectionPool Initialized with {}/{} connections.", _freeConnections.size(), _poolSize);
}

IDatabaseConnection *DBConnectionPool::Acquire()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_freeConnections.empty())
    {
        // Option: Expand pool? Or Wait?
        // For Phase 3 basic, return nullptr if exhausted (or implement expansion later)
        return nullptr;
    }

    IDatabaseConnection *conn = _freeConnections.front();
    _freeConnections.pop();

    // Basic Health Check
    if (!conn->IsConnected() || !conn->Ping())
    {
        LOG_INFO("Connection lost. Reconnecting...");
        conn->Disconnect();
        if (!conn->Connect(_connectionString))
        {
            LOG_ERROR("Failed to reconnect acquired connection.");
            // Broken connection, what to do?
            // For now, return it anyway or nullptr?
            // Better to return nullptr and let caller handle info, or generic retry logic.
            // Let's return nullptr to signal failure.
            // Release back to pool? If we release broken, next guy gets it.
            // Put back at end?
            _freeConnections.push(conn); // Put back
            return nullptr;
        }
    }

    return conn;
}

void DBConnectionPool::Release(IDatabaseConnection *conn)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _freeConnections.push(conn);
}

} // namespace System
