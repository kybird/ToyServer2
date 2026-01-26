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

    void HandlePacket(System::SessionContext ctx, System::PacketView packet) override;
    void OnSessionDisconnect(System::SessionContext ctx) override;

private:
    // [SessionContext Refactoring] Handler functions receive SessionContext by reference
    using PacketHandlerFunc = std::function<void(System::SessionContext &, System::PacketView)>;
    std::unordered_map<uint16_t, PacketHandlerFunc> _handlers;
};

} // namespace SimpleGame
