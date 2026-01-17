#include "System/Database/DatabaseImpl.h"
#include "System/Dispatcher/IDispatcher.h"
#ifdef USE_SQLITE
#include "System/Drivers/SQLite/SQLiteConnectionFactory.h"
#endif
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>

namespace System {

// Simple Mock Dispatcher for Testing
class TestDispatcher : public IDispatcher
{
public:
    void Push(std::function<void()> task) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _tasks.push_back(std::move(task));
        _cv.notify_one();
    }

    void Post(IMessage *) override
    {
    }

    bool Process() override
    {
        std::vector<std::function<void()>> currentTasks;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            currentTasks.swap(_tasks);
        }
        bool processed = !currentTasks.empty();
        for (auto &t : currentTasks)
            t();
        return processed;
    }

    bool WaitForTasks(int count, int timeoutMs = 2000)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _cv.wait_for(
            lock,
            std::chrono::milliseconds(timeoutMs),
            [this, count]
            {
                return _tasks.size() >= (size_t)count;
            }
        );
    }

    // Dummy methods
    void Wait(int) override
    {
    }
    size_t GetQueueSize() const override
    {
        return _tasks.size();
    }
    bool IsOverloaded() const override
    {
        return false;
    }
    bool IsRecovered() const override
    {
        return true;
    }
    void RegisterTimerHandler(ITimerHandler *) override
    {
    }

private:
    std::vector<std::function<void()>> _tasks;
    std::mutex _mutex;
    std::condition_variable _cv;
};

class DatabaseAsyncTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

TEST_F(DatabaseAsyncTest, BasicAsyncQuery)
{
    // 1. Create SQLite DB (memory) with 1 thread
#ifdef USE_SQLITE
    auto factory = std::make_unique<SQLiteConnectionFactory>();
    auto db = std::make_shared<DatabaseImpl>(":memory:", 1, 5000, std::move(factory), nullptr, nullptr);
    db->Init();
    ASSERT_NE(db, nullptr);
#else
    GTEST_SKIP() << "SQLite Driver Not Enabled";
    return;
#endif

    auto dispatcher = std::make_shared<TestDispatcher>();
    db->SetDispatcher(dispatcher);

    // 2. Setup Table (Sync)
    db->Execute("CREATE TABLE test (id INTEGER, val TEXT);");
    db->Execute("INSERT INTO test VALUES (1, 'hello');");

    // 3. Async Query
    std::atomic<bool> called{false};
    std::string resultValue;

    db->QueryAsync(
        "SELECT val FROM test WHERE id = 1;",
        [&](DbResult<std::unique_ptr<IResultSet>> res)
        {
            if (res.status.IsOk() && res.value.has_value())
            {
                auto rs = std::move(*res.value);
                if (rs->Next())
                {
                    resultValue = rs->GetString(0);
                }
            }
            called = true;
        }
    );

    // 4. Wait for worker and process callback
    EXPECT_TRUE(dispatcher->WaitForTasks(1));
    dispatcher->Process();

    EXPECT_TRUE(called);
    EXPECT_EQ(resultValue, "hello");
}

TEST_F(DatabaseAsyncTest, RunInTransactionAsync)
{
#ifdef USE_SQLITE
    auto factory = std::make_unique<SQLiteConnectionFactory>();
    auto db = std::make_shared<DatabaseImpl>(":memory:", 1, 5000, std::move(factory), nullptr, nullptr);
    db->Init();
    ASSERT_NE(db, nullptr);
#else
    GTEST_SKIP() << "SQLite Driver Not Enabled";
    return;
#endif

    auto dispatcher = std::make_shared<TestDispatcher>();
    db->SetDispatcher(dispatcher);

    db->Execute("CREATE TABLE balance (id INTEGER PRIMARY KEY, gold INTEGER);");
    db->Execute("INSERT INTO balance VALUES (1, 100);");

    // Complex task: check if gold > 50, then subtract 50
    std::atomic<bool> txSuccess{false};
    std::atomic<bool> called{false};

    db->RunInTransaction(
        [](IDatabase *db) -> bool
        {
            auto res = db->Query("SELECT gold FROM balance WHERE id = 1;");
            if (!res.status.IsOk() || !res.value.has_value())
                return false;

            auto rs = std::move(*res.value);
            if (!rs->Next() || rs->GetInt(0) < 50)
                return false;

            auto txRes = db->BeginTransaction();
            if (!txRes.status.IsOk() || !txRes.value.has_value())
                return false;
            auto tx = std::move(*txRes.value);

            if (!db->Execute("UPDATE balance SET gold = gold - 50 WHERE id = 1;").IsOk())
                return false;

            return tx->Commit().IsOk();
        },
        [&](bool success)
        {
            txSuccess = success;
            called = true;
        }
    );

    EXPECT_TRUE(dispatcher->WaitForTasks(1));
    dispatcher->Process();

    EXPECT_TRUE(called);
    EXPECT_TRUE(txSuccess);

    // Verify change (Sync query is fine for verification)
    auto finalRes = db->Query("SELECT gold FROM balance WHERE id = 1;");
    ASSERT_TRUE(finalRes.status.IsOk());
    auto rs = std::move(*finalRes.value);
    ASSERT_TRUE(rs->Next());
    EXPECT_EQ(rs->GetInt(0), 50);
}

} // namespace System
