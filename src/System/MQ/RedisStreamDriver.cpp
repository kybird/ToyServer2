#include "RedisStreamDriver.h"
#include "System/Pch.h"
#include <iostream>

namespace System::MQ {

RedisStreamDriver::RedisStreamDriver()
{
}

RedisStreamDriver::~RedisStreamDriver()
{
    Disconnect();
}

bool RedisStreamDriver::Connect(const std::string &connectionString)
{
    try
    {
        m_redis = std::make_unique<sw::redis::Redis>(connectionString);
        // Ping to check connection
        m_redis->ping();

        m_running = true;
        m_pollThread = std::thread(&RedisStreamDriver::_PollThread, this);
        return true;
    } catch (const std::exception &e)
    {
        // std::cerr << "Redis Connect Error: " << e.what() << std::endl;
        return false;
    }
}

void RedisStreamDriver::Disconnect()
{
    m_running = false;
    if (m_pollThread.joinable())
    {
        m_pollThread.join();
    }
    m_redis.reset();
}

bool RedisStreamDriver::Publish(const std::string &topic, const std::string &message)
{
    if (!m_redis)
        return false;
    try
    {
        // XADD topic * payload message
        // redis-plus-plus xadd expects an iterator range for attributes
        std::vector<std::pair<std::string, std::string>> attrs = {{"payload", message}};
        m_redis->xadd(topic, "*", attrs.begin(), attrs.end());
        return true;
    } catch (const std::exception &e)
    {
        // std::cerr << "Redis Publish Error: " << e.what() << std::endl;
        return false;
    }
}

bool RedisStreamDriver::Subscribe(const std::string &topic, MessageCallback callback)
{
    std::lock_guard<std::mutex> lock(m_subMutex);
    m_subs.push_back({topic, "$", callback});
    m_subsChanged = true;
    return true;
}

void RedisStreamDriver::_PollThread()
{
    while (m_running)
    {
        std::vector<Subscription> currentSubs;
        {
            std::lock_guard<std::mutex> lock(m_subMutex);
            currentSubs = m_subs;
        }

        if (currentSubs.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        try
        {
            // Prepare keys for XREAD: map<stream_name, id>
            std::unordered_map<std::string, std::string> streams;
            for (const auto &sub : currentSubs)
            {
                streams[sub.topic] = sub.lastId;
            }

            // XREAD output structure
            using Attrs = std::vector<std::pair<std::string, std::string>>;
            using Item = std::pair<std::string, sw::redis::Optional<Attrs>>; // ID, Attrs (Optional)
            using ItemStream = std::vector<Item>;
            using ResultMap = std::unordered_map<std::string, ItemStream>;

            ResultMap results;

            // Block for 100ms. xread expects (stream_range_begin, stream_range_end, timeout, count, output_iterator)
            // Note: older versions might differ, but typically it supports range.
            // If this fails, we might need a specific overload check.
            // Based on docs: xread(streams_begin, streams_end, block, count, inserter)
            m_redis->xread(
                streams.begin(),
                streams.end(),
                std::chrono::milliseconds(100),
                10,
                std::inserter(results, results.end())
            );

            if (!results.empty())
            {
                std::lock_guard<std::mutex> lock(m_subMutex);

                for (auto &streamPair : results)
                {
                    const std::string &topic = streamPair.first;
                    const auto &items = streamPair.second;

                    // Find subscription to update ID and callback
                    for (auto &mainSub : m_subs)
                    {
                        if (mainSub.topic == topic)
                        {
                            for (const auto &item : items)
                            {
                                const std::string &id = item.first;
                                mainSub.lastId = id; // Update Last ID

                                // Extract payload
                                if (item.second)
                                { // Check optional
                                    const auto &attrs = *item.second;
                                    for (const auto &attr : attrs)
                                    {
                                        if (attr.first == "payload")
                                        {
                                            mainSub.callback(topic, attr.second);
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }

        } catch (const std::exception &e)
        {
            // std::cerr << "Redis Poll Error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

} // namespace System::MQ
