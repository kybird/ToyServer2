#pragma once

#include "Protocol.h"
#include "System/Dispatcher/IPacketHandler.h"
#include <functional>
#include <unordered_map>

namespace SimpleGame {

class GamePacketHandler : public System::IPacketHandler
{
public:
    GamePacketHandler();
    virtual ~GamePacketHandler() = default;

    void HandlePacket(System::ISession *session, System::PacketView packet) override;
    void OnSessionDisconnect(System::ISession *session) override;

private:
    using PacketHandlerFunc = std::function<void(System::ISession*, System::PacketView)>;
    std::unordered_map<uint16_t, PacketHandlerFunc> _handlers;
};

} // namespace SimpleGame
