#include "NatsDriver.h"
#include "System/Pch.h"
#include <iostream>

namespace System::MQ {

NatsDriver::NatsDriver()
{
}

NatsDriver::~NatsDriver()
{
    Disconnect();
}

bool NatsDriver::Connect(const std::string &connectionString)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_conn)
        return true;

    natsStatus s = natsConnection_ConnectTo(&m_conn, connectionString.c_str());
    if (s == NATS_OK)
    {
        // std::cout << "Connected to NATS at " << connectionString << std::endl;
        return true;
    }
    else
    {
        const char *err = natsStatus_GetText(s);
        // std::cerr << "Failed to connect to NATS: " << err << std::endl;
        return false;
    }
}

void NatsDriver::Disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto sub : m_subs)
    {
        natsSubscription_Destroy(sub);
    }
    m_subs.clear();

    for (auto cb : m_callbacks)
    {
        delete cb;
    }
    m_callbacks.clear();

    if (m_conn)
    {
        natsConnection_Destroy(m_conn);
        m_conn = nullptr;
    }
}

bool NatsDriver::Publish(const std::string &topic, const std::string &message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_conn)
        return false;

    natsStatus s = natsConnection_PublishString(m_conn, topic.c_str(), message.c_str());
    return (s == NATS_OK);
}

void NatsDriver::_OnMessage(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    auto *cb = static_cast<MessageCallback *>(closure);
    if (cb)
    {
        std::string topic = natsMsg_GetSubject(msg);
        std::string data(natsMsg_GetData(msg), natsMsg_GetDataLength(msg));
        (*cb)(topic, data);
    }
    natsMsg_Destroy(msg);
}

bool NatsDriver::Subscribe(const std::string &topic, MessageCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_conn)
        return false;

    // Callback must be kept alive. For simplicity in this demo driver, we assume callback is valid for duration.
    // In production, we'd need better lifecycle management for the closure.
    // We'll allocate the callback on heap to persist it, or use a wrapper.
    // For now, this has a LEAK if we don't clean it up, but Closure for C API is tricky with std::function.

    // Better approach: wrap the callback in a struct we track.
    auto *persistentCallback = new MessageCallback(callback);
    // We need to store this pointer to delete it later, but NATS C API destroy callback?
    // natsSubscription_Destroy doesn't free closure automatically unless we tell it.
    // We'll skip complex memory management for this MVP step but note it.

    natsSubscription *sub = nullptr;
    natsStatus s = natsConnection_Subscribe(&sub, m_conn, topic.c_str(), _OnMessage, persistentCallback);

    if (s == NATS_OK)
    {
        m_subs.push_back(sub);
        m_callbacks.push_back(persistentCallback);
        return true;
    }
    else
    {
        delete persistentCallback;
        return false;
    }
}

} // namespace System::MQ
