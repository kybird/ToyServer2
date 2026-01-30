#include "System/Session/Session.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/ILog.h"
#include "System/Packet/IPacket.h"

namespace System {

Session::Session()
{
    _lastStatTime = std::chrono::steady_clock::now();
}

Session::~Session()
{
}

uint64_t Session::GetId() const
{
    return _id;
}

bool Session::IsConnected() const
{
    return _connected.load(std::memory_order_relaxed);
}

bool Session::CanDestroy() const
{
    return !IsConnected() && _ioRef.load(std::memory_order_acquire) == 0;
}

void Session::IncRef()
{
    _ioRef.fetch_add(1, std::memory_order_relaxed);
}

void Session::DecRef()
{
    _ioRef.fetch_sub(1, std::memory_order_release);
}

void Session::Reset()
{
    _connected.store(false);
    _ioRef.store(0);
    _isSending.store(false);

    // Clear send queue safely
    PacketMessage *msg = nullptr;
    while (_sendQueue.try_dequeue(msg))
    {
        MessagePool::Free(msg);
    }
}

void Session::SendPacket(const IPacket &pkt)
{
    if (!IsConnected())
        return;

    uint16_t size = pkt.GetTotalSize();
    auto msg = MessagePool::AllocatePacket(size);
    if (!msg)
        return;

    pkt.SerializeTo(msg->Payload());
    EnqueueSend(msg);
}

void Session::SendPacket(PacketPtr msg)
{
    if (!IsConnected() || !msg)
        return;

    EnqueueSend(msg.Release());
}

void Session::SendPreSerialized(const PacketMessage *msg)
{
    if (!IsConnected() || !msg)
        return;

    msg->AddRef();
    EnqueueSend(const_cast<PacketMessage *>(msg));
}

void Session::EnqueueSend(PacketMessage *msg)
{
    _sendQueue.enqueue(msg);
    if (_isSending.exchange(true, std::memory_order_acq_rel) == false)
    {
        Flush();
    }
}

} // namespace System
