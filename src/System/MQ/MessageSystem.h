#pragma once
#include "IMessageDriver.h"
#include "MessageQoS.h"
#include <map>
#include <memory>
#include <string>

namespace System::MQ {

class MessageSystem
{
public:
    static MessageSystem &Instance();

    bool Initialize(const std::string &natsConfig, const std::string &redisConfig);
    void Shutdown();

    bool Publish(const std::string &topic, const std::string &message, MessageQoS qos);
    bool Subscribe(const std::string &topic, MessageCallback callback, MessageQoS qos);

private:
    MessageSystem() = default;
    ~MessageSystem() = default;
    MessageSystem(const MessageSystem &) = delete;
    MessageSystem &operator=(const MessageSystem &) = delete;

    std::map<MessageQoS, std::shared_ptr<IMessageDriver>> m_drivers;
};

} // namespace System::MQ
