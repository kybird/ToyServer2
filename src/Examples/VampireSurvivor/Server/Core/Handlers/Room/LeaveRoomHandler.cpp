#include "LeaveRoomHandler.h"
#include "Core/GameEvents.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/Events/EventBus.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Room {

void LeaveRoomHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    Protocol::C_LeaveRoom req;
    if (packet.Parse(req))
    {
        uint64_t sessionId = ctx.Id();

        // Broadcast Event
        System::EventBus::Instance().Publish(RoomLeftEvent{sessionId});

        Protocol::S_LeaveRoom res;
        res.set_success(true);
        // Send Response
        S_LeaveRoomPacket respPacket(res);
        ctx.Send(respPacket);
        LOG_INFO("Session {} Left Room Event Published", sessionId);
    }
}

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
