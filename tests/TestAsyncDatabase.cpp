
#include "System/Database/DatabaseImpl.h"
#include "System/Drivers/SQLite/SQLiteConnectionFactory.h"
#include "System/Thread/ThreadPool.h"
#include "System/ToyServerSystem.h"
#include <condition_variable>
#include <deque>
#include <gtest/gtest.h>
#include <mutex>

// Mock Dispatcher to simulate Main Thread processing
class DbTestMockDispatcher : public System::IDispatcher
{
public:
    void Push(std::function<void()> task) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_back(task);
        _cv.notify_one();
    }

    void Post(System::IMessage *msg) override
    {
        // AsyncDatabaseImpl only uses Push for callbacks.
        // If it used Post, we'd need to handle it.
        delete msg;
    }

    bool Process() override
    {
        return false;
    }
    void Wait(int) override
    {
    }
    size_t GetQueueSize() const override
    {
        return _queue.size();
    }
    bool IsOverloaded() const override
    {
        return false;
    }
    bool IsRecovered() const override
    {
        return true;
    }
    void RegisterTimerHandler(System::ITimerHandler *handler) override
    {
    }

    void WithSession(uint64_t sessionId, std::function<void(System::SessionContext &)> callback) override
    {
        // Not used in DB tests
    }

    void Shutdown() override
    {
        _cv.notify_all();
    }

    void ProcessAll()
    {
        // Simple loop to process currently queued tasks
        // We do a small wait if empty to allow async thread to push
        std::unique_lock<std::mutex> lock(_mutex);
        if (_queue.empty())
            _cv.wait_for(lock, std::chrono::milliseconds(200));

        while (!_queue.empty())
        {
            auto task = _queue.front();
            _queue.pop_front();
            lock.unlock();
            task();
            lock.lock();
        }
    }

private:
    std::deque<std::function<void()>> _queue;
    std::mutex _mutex;
    std::condition_variable _cv;
};

class AsyncDatabaseTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 1. Setup ThreadPool
        _threadPool = std::make_shared<System::ThreadPool>(2);
        _threadPool->Start();

        // 2. Setup Mock Dispatcher
        _dispatcher = std::make_shared<DbTestMockDispatcher>();

        // 3. Create Database with dependencies injected
        auto factory = std::make_unique<System::SQLiteConnectionFactory>();
        auto dbImpl =
            std::make_shared<System::DatabaseImpl>(":memory:", 1, 5000, std::move(factory), _threadPool, _dispatcher);
        dbImpl->Init();
        _db = dbImpl;
        ASSERT_TRUE(_db);
        _db->Execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT);");

        // Use the same pointer for compatibility with existing test code variable
        _asyncDb = _db;
    }

    void TearDown() override
    {
        if (_threadPool)
            _threadPool->Stop();
    }

    std::shared_ptr<System::IDatabase> _db;
    std::shared_ptr<System::ThreadPool> _threadPool;
    std::shared_ptr<DbTestMockDispatcher> _dispatcher;
    std::shared_ptr<System::IDatabase> _asyncDb; // Just an alias now
};

TEST_F(AsyncDatabaseTest, TestInsertAndQuery)
{
    // Async Insert
    bool finished = false;
    _asyncDb->AsyncExecute(
        "INSERT INTO test (value) VALUES ('HelloAsync');",
        [&](System::DbStatus status)
        {
            ASSERT_TRUE(status.IsOk());
            finished = true;
        }
    );

    // Process Callback
    _dispatcher->ProcessAll();
    ASSERT_TRUE(finished);

    // Async Query
    finished = false;
    _asyncDb->AsyncQuery(
        "SELECT value FROM test WHERE value='HelloAsync';",
        [&](auto res)
        {
            ASSERT_TRUE(res.status.IsOk());
            ASSERT_TRUE(res.value.has_value());

            auto rs = std::move(*res.value);
            ASSERT_TRUE(rs->Next());
            ASSERT_EQ(rs->GetString(0), "HelloAsync");

            finished = true;
        }
    );

    _dispatcher->ProcessAll();
    ASSERT_TRUE(finished);
}

TEST_F(AsyncDatabaseTest, TestRunInTransaction)
{
    bool finished = false;

    _asyncDb->AsyncRunInTransaction(
        [](System::IDatabase *db)
        {
            db->Execute("INSERT INTO test (value) VALUES ('Tx1');");
            db->Execute("INSERT INTO test (value) VALUES ('Tx2');");
            return true;
        },
        [&](bool success)
        {
            ASSERT_TRUE(success);
            finished = true;
        }
    );

    _dispatcher->ProcessAll();
    ASSERT_TRUE(finished);

    // Verify with sync query that data is there
    auto res = _db->Query("SELECT COUNT(*) FROM test WHERE value LIKE 'Tx%';");
    ASSERT_TRUE(res.value.has_value());
    ASSERT_EQ(res.value.value()->GetInt(0), 2);
}
