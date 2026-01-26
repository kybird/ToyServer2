#include "System/Session/SessionContext.h"
#include "System/ISession.h"

namespace System {

SessionContext::SessionContext(ISession *session) : _session(session)
{
}

SessionContext::SessionContext(SessionContext &&other) noexcept : _session(other._session)
{
    other._session = nullptr;
}

SessionContext &SessionContext::operator=(SessionContext &&other) noexcept
{
    if (this != &other)
    {
        _session = other._session;
        other._session = nullptr;
    }
    return *this;
}

uint64_t SessionContext::Id() const
{
    return _session != nullptr ? _session->GetId() : 0;
}

bool SessionContext::IsConnected() const
{
    return _session != nullptr && _session->IsConnected();
}

void SessionContext::Send(const IPacket &pkt)
{
    if (_session != nullptr && _session->IsConnected())
    {
        _session->SendPacket(pkt);
    }
}

void SessionContext::Send(PacketPtr msg)
{
    if (_session != nullptr && _session->IsConnected())
    {
        _session->SendPacket(msg);
    }
}

void SessionContext::Close()
{
    if (_session != nullptr)
    {
        _session->Close();
    }
}

void SessionContext::OnPong()
{
    if (_session != nullptr)
    {
        _session->OnPong();
    }
}

} // namespace System
