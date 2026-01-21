#pragma once
#include "IMessageDriver.h"
#include <atomic>
#include <mutex>
#include <sw/redis++/redis++.h>
#include <unordered_map>
#include <vector>

namespace System {
class ThreadPool;
}

namespace System::MQ {

class RedisStreamDriver : public IMessageDriver
{
public:
    RedisStreamDriver();
    ~RedisStreamDriver() override;

    bool Connect(const std::string &connectionString) override;
    void Disconnect() override;

    bool Publish(const std::string &topic, const std::string &message) override;
    bool Subscribe(const std::string &topic, MessageCallback callback) override;

    void SetThreadPool(System::ThreadPool *threadPool) override;

private:
    void _PollTask();

    std::unique_ptr<sw::redis::Redis> m_redis;
    std::atomic<bool> m_running{false};
    System::ThreadPool *m_threadPool = nullptr;

    struct Subscription
    {
        std::string topic;
        std::string lastId = "$"; // Start from new messages
        MessageCallback callback;
    };

    std::mutex m_subMutex;
    std::vector<Subscription> m_subs;
    bool m_subsChanged = false;
};

} // namespace System::MQ
