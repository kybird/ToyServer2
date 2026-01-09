#pragma once

#include "System/Database/IConnection.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/IDatabase.h"
#include "System/Thread/IThreadPool.h"
#include <atomic>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace System {

class DatabaseImpl : public IDatabase, public std::enable_shared_from_this<DatabaseImpl>
{
public:
    using ConnectionFactory = std::function<IConnection *()>;

    DatabaseImpl(const std::string &connStr, int poolSize, int defaultTimeoutMs, ConnectionFactory factory);
    ~DatabaseImpl() override;

    void Init();

    // IDatabase 구현
    DbResult<std::unique_ptr<IResultSet>> Query(const std::string &sql) override;
    DbStatus Execute(const std::string &sql) override;
    DbResult<std::unique_ptr<IPreparedStatement>> Prepare(const std::string &sql) override;
    DbResult<std::unique_ptr<ITransaction>> BeginTransaction() override;

    void SetDispatcher(std::shared_ptr<IDispatcher> dispatcher) override
    {
        _dispatcher = std::move(dispatcher);
    }

    void QueryAsync(
        const std::string &sql, std::function<void(DbResult<std::unique_ptr<IResultSet>>)> callback,
        int timeoutMs = -1
    ) override;
    void ExecuteAsync(const std::string &sql, std::function<void(DbStatus)> callback, int timeoutMs = -1) override;

    void RunInTransaction(std::function<bool(IDatabase *)> transactionFunc, std::function<void(bool)> callback) override;

    // 내부용
    std::shared_ptr<IConnection> Acquire(int timeoutMs);
    void Release(IConnection *conn);

private:
    std::string _connectionString;
    int _poolMax;
    int _defaultTimeoutMs;
    ConnectionFactory _factory;

    std::shared_ptr<IDispatcher> _dispatcher;
    std::shared_ptr<IThreadPool> _workerPool;

    moodycamel::ConcurrentQueue<IConnection *> _pool;
    std::atomic<int> _currentSize{0};

    // 대기용 (timeout_ms > 0 인 경우)
    std::mutex _waitMutex;
    std::condition_variable _waitCv;
    std::atomic<int> _waitingThreads{0};
};

/**
 * PreparedStatement Wrapper
 * - 커넥션의 shared_ptr를 소유하여 수명을 연장
 */
class PreparedStatementWrapper : public IPreparedStatement
{
public:
    PreparedStatementWrapper(std::shared_ptr<IConnection> conn, std::unique_ptr<IPreparedStatement> inner)
        : _conn(std::move(conn)), _inner(std::move(inner))
    {
    }

    DbStatus BindInt(int idx, int val) override
    {
        return _inner->BindInt(idx, val);
    }
    DbStatus BindString(int idx, const std::string &val) override
    {
        return _inner->BindString(idx, val);
    }
    DbStatus BindDouble(int idx, double val) override
    {
        return _inner->BindDouble(idx, val);
    }

    DbResult<std::unique_ptr<IResultSet>> ExecuteQuery() override
    {
        return _inner->ExecuteQuery();
    }
    DbStatus ExecuteUpdate() override
    {
        return _inner->ExecuteUpdate();
    }

private:
    std::shared_ptr<IConnection> _conn;
    std::unique_ptr<IPreparedStatement> _inner;
};

/**
 * Transaction Wrapper
 * - 커넥션의 shared_ptr를 소유하여 수명을 연장
 */
class TransactionWrapper : public ITransaction
{
public:
    TransactionWrapper(std::shared_ptr<IConnection> conn) : _conn(std::move(conn))
    {
    }

    ~TransactionWrapper() override;

    DbStatus Commit() override
    {
        if (_committed)
            return DbStatus::Error("Already committed");

        auto status = _conn->Commit();
        if (status.IsOk())
            _committed = true;
        return status;
    }

private:
    std::shared_ptr<IConnection> _conn;
    bool _committed = false;
};

/**
 * Result Set Wrapper
 * - 커넥션의 shared_ptr를 소유하여 수명을 연장
 */
class ResultSetWrapper : public IResultSet
{
public:
    ResultSetWrapper(std::shared_ptr<IConnection> conn, std::unique_ptr<IResultSet> inner)
        : _conn(std::move(conn)), _inner(std::move(inner))
    {
    }

    bool Next() override
    {
        return _inner->Next();
    }
    int GetInt(int idx) override
    {
        return _inner->GetInt(idx);
    }
    std::string GetString(int idx) override
    {
        return _inner->GetString(idx);
    }
    double GetDouble(int idx) override
    {
        return _inner->GetDouble(idx);
    }

private:
    std::shared_ptr<IConnection> _conn;
    std::unique_ptr<IResultSet> _inner;
};

} // namespace System
