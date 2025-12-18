#pragma once

#include "Core/Protocol.h"
#include "System/Dispatcher/IPacketHandler.h"

namespace SimpleGame {

class GamePacketHandler : public System::IPacketHandler
{
public:
    void HandlePacket(System::ISession *session, System::PacketView packet) override;
};

} // namespace SimpleGame
