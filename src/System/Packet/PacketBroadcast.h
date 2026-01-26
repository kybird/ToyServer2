#pragma once
#include "System/ILog.h"
#include "System/ISession.h"
#include "System/Packet/IPacket.h"
#include "System/Packet/PacketBuilder.h"
#include <memory>
#include <span>
#include <vector>


namespace System {

class PacketBroadcast
{
public:
    static void Broadcast(const IPacket &pkt, std::span<ISession *> sessions)
    {
        // LOG_INFO("Packet::Broadcast");
        if (sessions.empty())
            return;

        // 1. Serialize Once (Template)
        PacketMessage *msg = PacketBuilder::Build(pkt);
        if (!msg)
            return;

        // 2. Loop & Send (Copy)
        for (ISession *s : sessions)
        {
            if (s && s->CanDestroy() == false)
            {
                s->SendPreSerialized(msg);
            }
        }

        // 3. Free Template
        System::MessagePool::Free(msg);
        // LOG_INFO("Packet::Broadcast done");
    }

    // Helper for shared_ptr
    static void Broadcast(const IPacket &pkt, const std::vector<std::shared_ptr<ISession>> &sessions)
    {
        if (sessions.empty())
            return;

        PacketMessage *msg = PacketBuilder::Build(pkt);
        if (!msg)
            return;

        for (const auto &s : sessions)
        {
            if (s && s->CanDestroy() == false)
            {
                s->SendPreSerialized(msg);
            }
        }

        System::MessagePool::Free(msg);
    }
};

} // namespace System
