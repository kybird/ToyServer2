#include "System/Database/DatabaseImpl.h"
#include "System/ILog.h"
#include "System/Thread/ThreadPool.h"
#include <chrono>
#include <thread>

namespace System {

// SQLite 드라이버용 팩토리 (향후 드라이버 추가 시 확장)
// 이 부분은 드라이버 구현 후에 구체화되거나 IDatabase::Create에서 처리됨.
IConnection *CreateSQLiteConnection();

std::shared_ptr<IDatabase>
IDatabase::Create(const std::string &driverType, const std::string &connStr, int poolSize, int defaultTimeoutMs)
{
    DatabaseImpl::ConnectionFactory factory;

    if (driverType == "sqlite")
    {
        factory = []()
        {
            // SQLiteConnection을 생성하여 반환 (내부 인터페이스 IConnection으로 캐스팅 필요)
            // 실제 구현은 SQLiteConnection.cpp에서 제공하는 헬퍼를 활용할 예정
            return CreateSQLiteConnection();
        };
    }
    else
    {
        return nullptr;
    }

    auto db = std::make_shared<DatabaseImpl>(connStr, poolSize, defaultTimeoutMs, factory);
    db->Init();
    return db;
}

DatabaseImpl::DatabaseImpl(const std::string &connStr, int poolSize, int defaultTimeoutMs, ConnectionFactory factory)
    : _connectionString(connStr), _poolMax(poolSize), _defaultTimeoutMs(defaultTimeoutMs), _factory(factory)
{
    _workerPool = std::make_shared<ThreadPool>(poolSize);
}

DatabaseImpl::~DatabaseImpl()
{
    if (_workerPool)
        _workerPool->Stop();

    IConnection *conn = nullptr;
    while (_pool.try_dequeue(conn))
    {
        if (conn)
        {
            conn->Disconnect();
            delete conn;
        }
    }
}

void DatabaseImpl::Init()
{
    if (_workerPool)
        _workerPool->Start();

    // 초기 커넥션 생성 (선택 사항이나 권장)
    // 여기서는 최소 1개는 생성해보고 성공 여부 확인
    auto conn = Acquire(0);
    if (!conn)
    {
        LOG_ERROR("DatabaseImpl: Failed to create initial connection.");
    }
}

void DatabaseImpl::QueryAsync(
    const std::string &sql, std::function<void(DbResult<std::unique_ptr<IResultSet>>)> callback, int timeoutMs
)
{
    if (!_dispatcher)
    {
        LOG_ERROR("DatabaseImpl::QueryAsync: Dispatcher not set!");
        return;
    }

    _workerPool->Submit(
        [this, sql, callback, timeoutMs]() mutable
        {
            auto result = std::make_shared<DbResult<std::unique_ptr<IResultSet>>>(this->Query(sql));
            _dispatcher->Push(
                [callback, result]() mutable
                {
                    callback(std::move(*result));
                }
            );
        }
    );
}

void DatabaseImpl::ExecuteAsync(const std::string &sql, std::function<void(DbStatus)> callback, int timeoutMs)
{
    if (!_dispatcher)
    {
        LOG_ERROR("DatabaseImpl::ExecuteAsync: Dispatcher not set!");
        return;
    }

    _workerPool->Submit(
        [this, sql, callback, timeoutMs]()
        {
            auto status = this->Execute(sql);
            _dispatcher->Push(
                [callback, status]()
                {
                    callback(status);
                }
            );
        }
    );
}

// Connection-pinned Database Proxy for transactions
class DatabaseConnectionProxy : public IDatabase
{
public:
    DatabaseConnectionProxy(DatabaseImpl *owner, std::shared_ptr<IConnection> conn) : _owner(owner), _conn(conn)
    {
    }

    DbResult<std::unique_ptr<IResultSet>> Query(const std::string &sql) override
    {
        auto res = _conn->Query(sql);
        if (res.status.IsOk() && res.value.has_value())
        {
            auto wrapper = std::make_unique<ResultSetWrapper>(_conn, std::move(*res.value));
            return DbResult<std::unique_ptr<IResultSet>>::Success(std::move(wrapper));
        }
        return res;
    }

    DbStatus Execute(const std::string &sql) override
    {
        return _conn->Execute(sql);
    }

    DbResult<std::unique_ptr<IPreparedStatement>> Prepare(const std::string &sql) override
    {
        auto innerResult = _conn->Prepare(sql);
        if (!innerResult.status.IsOk())
            return innerResult;
        auto wrapper = std::make_unique<PreparedStatementWrapper>(_conn, std::move(*innerResult.value));
        return DbResult<std::unique_ptr<IPreparedStatement>>::Success(std::move(wrapper));
    }

    DbResult<std::unique_ptr<ITransaction>> BeginTransaction() override
    {
        auto status = _conn->BeginTransaction();
        if (!status.IsOk())
            return DbResult<std::unique_ptr<ITransaction>>::Fail(status.code, status.message);
        auto wrapper = std::make_unique<TransactionWrapper>(_conn);
        return DbResult<std::unique_ptr<ITransaction>>::Success(std::move(wrapper));
    }

    void SetDispatcher(std::shared_ptr<IDispatcher> dispatcher) override
    {
        _owner->SetDispatcher(dispatcher);
    }

    void QueryAsync(const std::string &, std::function<void(DbResult<std::unique_ptr<IResultSet>>)> , int ) override
    {
        // Not supported/needed inside transaction-pinned proxy for now
    }
    void ExecuteAsync(const std::string &, std::function<void(DbStatus)> , int ) override
    {
        // Not supported
    }
    void RunInTransaction(std::function<bool(IDatabase *)> , std::function<void(bool)> ) override
    {
        // Not supported (Nested RunInTransaction)
    }

private:
    DatabaseImpl *_owner;
    std::shared_ptr<IConnection> _conn;
};

void DatabaseImpl::RunInTransaction(std::function<bool(IDatabase *)> transactionFunc, std::function<void(bool)> callback)
{
    if (!_dispatcher)
    {
        LOG_ERROR("DatabaseImpl::RunInTransaction: Dispatcher not set!");
        return;
    }

    _workerPool->Submit(
        [this, transactionFunc, callback]()
        {
            auto conn = this->Acquire(_defaultTimeoutMs);
            bool success = false;
            if (conn)
            {
                DatabaseConnectionProxy proxy(this, conn);
                success = transactionFunc(&proxy);
            }
            else
            {
                LOG_ERROR("DatabaseImpl::RunInTransaction: Failed to acquire connection.");
            }

            _dispatcher->Push(
                [callback, success]()
                {
                    if (callback)
                        callback(success);
                }
            );
        }
    );
}

std::shared_ptr<IConnection> DatabaseImpl::Acquire(int timeoutMs)
{
    IConnection *rawConn = nullptr;
    auto start = std::chrono::steady_clock::now();
    bool useInfiniteWait = (timeoutMs == -1);

    while (true)
    {
        // 1. Try to get from pool
        if (_pool.try_dequeue(rawConn))
        {
            // Validation for pooled connection
            if (rawConn->IsConnected() && rawConn->Ping())
            {
                // Success - Setup deleter below
                break;
            }

            // Invalid pooled connection, destroy and retry loop (create new or wait)
            rawConn->Disconnect();
            delete rawConn;
            _currentSize--;
            rawConn = nullptr;
            continue;
        }

        // 2. Try to create new if under limit
        {
            int current = _currentSize.load();
            if (current < _poolMax)
            {
                if (_currentSize.compare_exchange_weak(current, current + 1))
                {
                    rawConn = _factory();
                    if (rawConn && rawConn->Connect(_connectionString))
                    {
                        // Success (Fresh connection, no Ping needed)
                        break;
                    }

                    // Creation failed
                    if (rawConn)
                        delete rawConn;
                    _currentSize--;
                    _waitCv.notify_one(); // Notify others to try
                    return nullptr;
                }
                // CAS failed, retry loop
                continue;
            }
        }

        // 3. Wait if pool is full/empty
        if (timeoutMs == 0)
            return nullptr; // Non-blocking

        {
            std::unique_lock<std::mutex> lock(_waitMutex);

            // RAII for waiting threads count
            struct ScopedWaiter
            {
                std::atomic<int> &c;
                ScopedWaiter(std::atomic<int> &counter) : c(counter)
                {
                    c++;
                }
                ~ScopedWaiter()
                {
                    c--;
                }
            } waiter(_waitingThreads);

            bool success = false;
            auto predicate = [this, &rawConn]()
            {
                if (_pool.try_dequeue(rawConn))
                    return true;
                return _currentSize.load() < _poolMax;
            };

            if (useInfiniteWait)
            {
                _waitCv.wait(lock, predicate);
                success = true; // wait returns void, but if predicate matches we good
            }
            else
            {
                auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
                        .count();
                auto remaining = timeoutMs - elapsed;

                if (remaining <= 0)
                    return nullptr; // Timeout

                success = _waitCv.wait_for(lock, std::chrono::milliseconds(remaining), predicate);
            }

            if (!success)
                return nullptr; // Timeout

            if (rawConn)
            {
                // Dequeued from pool -> Validate
                if (rawConn->IsConnected() && rawConn->Ping())
                {
                    break;
                }
                // Invalid, cleanup and retry
                rawConn->Disconnect();
                delete rawConn;
                _currentSize--;
                rawConn = nullptr;
                continue;
            }

            // Woke up because space is available, retry loop to create
            continue;
        }
    }

    // Common Exit: Setup RAII deleter
    std::weak_ptr<DatabaseImpl> weakDb = shared_from_this();
    return std::shared_ptr<IConnection>(
        rawConn,
        [weakDb](IConnection *p)
        {
            if (auto db = weakDb.lock())
            {
                db->Release(p);
            }
            else
            {
                p->Disconnect();
                delete p;
            }
        }
    );
}

void DatabaseImpl::Release(IConnection *conn)
{
    if (!conn)
        return;

    // 상태 초기화 (트랜잭션 롤백 등)
    conn->ResetState();

    _pool.enqueue(conn);

    // 대기 중인 스레드가 있다면 알림
    if (_waitingThreads > 0)
    {
        _waitCv.notify_one();
    }
}

// --------------------------------------------------------------------------
// IDatabase 구현 (Helper 래퍼들)
// --------------------------------------------------------------------------

DbResult<std::unique_ptr<IResultSet>> DatabaseImpl::Query(const std::string &sql)
{
    auto conn = Acquire(_defaultTimeoutMs);
    if (!conn)
        return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_TIMEOUT, "Connection acquisition timeout");

    try
    {
        auto res = conn->Query(sql);
        if (res.status.IsOk() && res.value.has_value())
        {
            auto wrapper = std::make_unique<ResultSetWrapper>(conn, std::move(*res.value));
            return DbResult<std::unique_ptr<IResultSet>>::Success(std::move(wrapper));
        }
        return res;
    } catch (...)
    {
        return DbResult<std::unique_ptr<IResultSet>>::Fail(DbStatusCode::DB_ERROR, "Internal driver exception");
    }
}

DbStatus DatabaseImpl::Execute(const std::string &sql)
{
    auto conn = Acquire(_defaultTimeoutMs);
    if (!conn)
        return {DbStatusCode::DB_TIMEOUT, "Connection acquisition timeout"};

    try
    {
        return conn->Execute(sql);
    } catch (...)
    {
        return {DbStatusCode::DB_ERROR, "Internal driver exception"};
    }
}

DbResult<std::unique_ptr<IPreparedStatement>> DatabaseImpl::Prepare(const std::string &sql)
{
    auto conn = Acquire(_defaultTimeoutMs);
    if (!conn)
        return DbResult<std::unique_ptr<IPreparedStatement>>::Fail(
            DbStatusCode::DB_TIMEOUT, "Connection acquisition timeout"
        );

    try
    {
        auto innerResult = conn->Prepare(sql);
        if (!innerResult.status.IsOk())
        {
            return innerResult;
        }

        auto wrapper = std::make_unique<PreparedStatementWrapper>(conn, std::move(*innerResult.value));

        return DbResult<std::unique_ptr<IPreparedStatement>>::Success(std::move(wrapper));
    } catch (...)
    {
        return DbResult<std::unique_ptr<IPreparedStatement>>::Fail(DbStatusCode::DB_ERROR, "Internal driver exception");
    }
}

DbResult<std::unique_ptr<ITransaction>> DatabaseImpl::BeginTransaction()
{
    auto conn = Acquire(_defaultTimeoutMs);
    if (!conn)
        return DbResult<std::unique_ptr<ITransaction>>::Fail(
            DbStatusCode::DB_TIMEOUT, "Connection acquisition timeout"
        );

    try
    {
        // 1. IConnection::BeginTransaction() 호출
        auto status = conn->BeginTransaction();
        if (!status.IsOk())
        {
            return DbResult<std::unique_ptr<ITransaction>>::Fail(status.code, status.message);
        }

        // 2. 래퍼 생성
        auto wrapper = std::make_unique<TransactionWrapper>(conn);

        // 3. 래퍼 반환
        return DbResult<std::unique_ptr<ITransaction>>::Success(std::move(wrapper));
    } catch (...)
    {
        return DbResult<std::unique_ptr<ITransaction>>::Fail(DbStatusCode::DB_ERROR, "Internal driver exception");
    }
}

// --------------------------------------------------------------------------
// Async Implementations
// --------------------------------------------------------------------------

void DatabaseImpl::ConfigureAsync(std::shared_ptr<ThreadPool> threadPool, std::shared_ptr<IDispatcher> dispatcher)
{
    _threadPool = threadPool;
    _dispatcher = dispatcher;
}

void DatabaseImpl::AsyncQuery(const std::string &sql, AsyncQueryCallback callback)
{
    if (!_threadPool)
    {
        if (callback && _dispatcher)
        {
            _dispatcher->Push(
                [callback]()
                {
                    callback(
                        DbResult<std::unique_ptr<IResultSet>>::Fail(
                            DbStatusCode::DB_ERROR, "Async Context Not Configured"
                        )
                    );
                }
            );
        }
        return;
    }

    // Capture shared_from_this() to ensure lifetime
    _threadPool->Enqueue(
        [self = shared_from_this(), sql, callback, dispatcher = _dispatcher]()
        {
            // [Worker Thread] Blocking Call
            auto result = self->Query(sql);

            // Wrap move-only result in shared_ptr
            auto sharedResult = std::make_shared<DbResult<std::unique_ptr<IResultSet>>>(std::move(result));

            dispatcher->Push(
                [callback, sharedResult]()
                {
                    // [Main Thread]
                    callback(std::move(*sharedResult));
                }
            );
        }
    );
}

void DatabaseImpl::AsyncExecute(const std::string &sql, AsyncExecCallback callback)
{
    if (!_threadPool)
    {
        if (callback && _dispatcher)
        {
            _dispatcher->Push(
                [callback]()
                {
                    callback(DbStatus::Error("Async Context Not Configured"));
                }
            );
        }
        return;
    }

    _threadPool->Enqueue(
        [self = shared_from_this(), sql, callback, dispatcher = _dispatcher]()
        {
            auto status = self->Execute(sql);

            dispatcher->Push(
                [callback, status]()
                {
                    callback(status);
                }
            );
        }
    );
}

void DatabaseImpl::AsyncRunInTransaction(std::function<bool(IDatabase *)> txLogic, std::function<void(bool)> callback)
{
    if (!_threadPool)
    {
        if (callback && _dispatcher)
        {
            _dispatcher->Push(
                [callback]()
                {
                    callback(false);
                }
            );
        }
        return;
    }

    _threadPool->Enqueue(
        [self = shared_from_this(), txLogic, callback, dispatcher = _dispatcher]()
        {
            // [Worker Thread]
            bool success = false;
            try
            {
                // Pass 'this' (self) as the Sync Database pointer
                success = txLogic(self.get());
            } catch (...)
            {
                success = false;
            }

            dispatcher->Push(
                [callback, success]()
                {
                    // [Main Thread]
                    callback(success);
                }
            );
        }
    );
}

} // namespace System

// --------------------------------------------------------------------------
// Implementation moved to avoid header dependency on ILog.h
// --------------------------------------------------------------------------
System::TransactionWrapper::~TransactionWrapper()
{
    if (!_committed && _conn)
    {
        try
        {
            auto status = _conn->Rollback();
            if (!status.IsOk())
            {
                LOG_ERROR("TransactionWrapper: Rollback failed in destructor. Msg: {}", status.message);
            }
        } catch (...)
        {
            LOG_ERROR("TransactionWrapper: Exception occurred during Rollback in destructor.");
        }
    }
}
