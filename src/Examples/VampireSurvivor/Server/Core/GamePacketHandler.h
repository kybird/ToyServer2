#pragma once

#include "Protocol.h"
#include "System/Dispatcher/IPacketHandler.h"

namespace SimpleGame {

class GamePacketHandler : public System::IPacketHandler
{
public:
    void HandlePacket(System::ISession *session, System::PacketView packet) override;
    void OnSessionDisconnect(System::ISession *session) override;
};

} // namespace SimpleGame
