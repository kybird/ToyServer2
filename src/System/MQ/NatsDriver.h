#pragma once
#include "IMessageDriver.h"
#include <mutex>
#include <nats/nats.h>
#include <vector>

namespace System::MQ {

class NatsDriver : public IMessageDriver
{
public:
    NatsDriver();
    ~NatsDriver() override;

    bool Connect(const std::string &connectionString) override;
    void Disconnect() override;

    bool Publish(const std::string &topic, const std::string &message) override;
    bool Subscribe(const std::string &topic, MessageCallback callback) override;

private:
    static void _OnMessage(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure);

    natsConnection *m_conn = nullptr;
    std::mutex m_mutex;
    std::vector<natsSubscription *> m_subs;
    std::vector<MessageCallback *> m_callbacks; // Track dynamically allocated callbacks
};

} // namespace System::MQ
