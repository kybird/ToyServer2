#include "System/Session/UDP/KCPAdapter.h"
#include "System/ILog.h"
#include "System/Pch.h"

namespace System {

KCPAdapter::KCPAdapter(uint32_t conv)
{
    _kcp = ikcp_create(conv, nullptr);

    if (!_kcp)
    {
        LOG_ERROR("[KCPAdapter] Failed to create KCP instance");
    }
    else
    {
        ikcp_nodelay((ikcpcb*)_kcp, 1, 20, 2, 1);
        ikcp_wndsize((ikcpcb*)_kcp, 128, 128);
        LOG_DEBUG("[KCPAdapter] Created with conv={}", conv);
    }
}

KCPAdapter::~KCPAdapter()
{
    if (_kcp)
    {
        ikcp_release((ikcpcb*)_kcp);
        _kcp = nullptr;
    }
}

int KCPAdapter::Send(const void *data, int length)
{
    if (!_kcp)
    {
        return -1;
    }

    return ikcp_send((ikcpcb*)_kcp, static_cast<const char *>(data), length);
}

int KCPAdapter::Input(const void *data, int length)
{
    if (!_kcp)
    {
        return -1;
    }

    return ikcp_input((ikcpcb*)_kcp, static_cast<const char *>(data), static_cast<long>(length));
}

void KCPAdapter::Update(uint32_t current)
{
    if (_kcp)
    {
        ikcp_update((ikcpcb*)_kcp, current);
    }
}

int KCPAdapter::Output(uint8_t *buffer, int maxSize)
{
    if (!_kcp)
    {
        return 0;
    }

    ikcp_flush((ikcpcb*)_kcp);
    return 0;
}

int KCPAdapter::Recv(uint8_t *buffer, int maxSize)
{
    if (!_kcp)
    {
        return -1;
    }

    return ikcp_recv((ikcpcb*)_kcp, reinterpret_cast<char *>(buffer), maxSize);
}

} // namespace System
