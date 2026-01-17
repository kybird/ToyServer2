#include "MessageSystem.h"
#include "NatsDriver.h"
#include "RedisStreamDriver.h"
#include "System/Pch.h"


namespace System::MQ {

MessageSystem &MessageSystem::Instance()
{
    static MessageSystem instance;
    return instance;
}

bool MessageSystem::Initialize(const std::string &natsConfig, const std::string &redisConfig)
{
    // Fast Channel (NATS)
    auto nats = std::make_shared<NatsDriver>();
    if (nats->Connect(natsConfig))
    {
        m_drivers[MessageQoS::Fast] = nats;
    }
    else
    {
        // Log warning?
        // return false; // For now we allow partial failure or decide later
    }

    // Reliable Channel (Redis)
    auto redis = std::make_shared<RedisStreamDriver>();
    if (redis->Connect(redisConfig))
    {
        m_drivers[MessageQoS::Reliable] = redis;
    }

    return !m_drivers.empty();
}

void MessageSystem::Shutdown()
{
    for (auto &pair : m_drivers)
    {
        pair.second->Disconnect();
    }
    m_drivers.clear();
}

bool MessageSystem::Publish(const std::string &topic, const std::string &message, MessageQoS qos)
{
    auto it = m_drivers.find(qos);
    if (it != m_drivers.end())
    {
        return it->second->Publish(topic, message);
    }
    return false;
}

bool MessageSystem::Subscribe(const std::string &topic, MessageCallback callback, MessageQoS qos)
{
    auto it = m_drivers.find(qos);
    if (it != m_drivers.end())
    {
        return it->second->Subscribe(topic, callback);
    }
    return false;
}

} // namespace System::MQ
