#pragma once

#include "System/Pch.h"
#include "System/ISession.h"
#include "System/Session/UDPSession.h"
#include "System/Session/GatewaySession.h"
#include "System/Session/BackendSession.h"
#include <concurrentqueue/moodycamel/concurrentqueue.h>

namespace System {

template<typename T>
class SessionPoolBase
{
public:
    static constexpr size_t POOL_SIZE = 32;

    SessionPoolBase()
    {
        for (size_t i = 0; i < POOL_SIZE; ++i)
        {
            _pool.enqueue(new T());
        }
    }

    ~SessionPoolBase()
    {
        T *session;
        while (_pool.try_dequeue(session))
        {
            delete session;
        }
    }

    T *Acquire()
    {
        T *session;
        if (_pool.try_dequeue(session))
        {
            return session;
        }
        return nullptr;
    }

    void Release(T *session)
    {
        if (session)
        {
            _pool.enqueue(session);
        }
    }

private:
    moodycamel::ConcurrentQueue<T*> _pool;
};

template<typename T>
SessionPoolBase<T>& GetSessionPool()
{
    static SessionPoolBase<T> instance;
    return instance;
}

using SessionPoolGateway = SessionPoolBase<GatewaySession>;
using SessionPoolBackend = SessionPoolBase<BackendSession>;
using SessionPoolUDP = SessionPoolBase<UDPSession>;

} // namespace System
