#pragma once
#include <functional>
#include <string>

namespace System {
class ThreadPool;
}

namespace System::MQ {

using MessageCallback = std::function<void(const std::string &topic, const std::string &message)>;

class IMessageDriver
{
public:
    virtual ~IMessageDriver() = default;

    // Connect to the backend
    virtual bool Connect(const std::string &connectionString) = 0;
    virtual void Disconnect() = 0;

    // Publish a message to a topic
    virtual bool Publish(const std::string &topic, const std::string &message) = 0;

    // Subscribe to a topic
    virtual bool Subscribe(const std::string &topic, MessageCallback callback) = 0;

    // Set execution context (ThreadPool) for blocking I/O drivers
    // Default implementation is no-op for backward compatibility
    virtual void SetThreadPool(System::ThreadPool *threadPool)
    {
    }
};

} // namespace System::MQ
