#include "LeaveRoomHandler.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
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
        auto player = RoomManager::Instance().GetPlayer(sessionId);
        if (player)
        {
            // Remove from Room
            // Assuming Player has pointer to Room or we need to search?
            // RoomManager doesn't track RoomID for player directly in hash map, but Player obj might know?
            // Checking Player.h would be safer, but for MVP assuming RoomManager::RegisterPlayer
            // implies active game. We should unregister.

            // Logic to remove from actual Room object is needed if Room holds shared_ptr<Player> list.
            int roomId = player->GetRoomId();
            auto room = RoomManager::Instance().GetRoom(roomId);
            if (room)
            {
                room->Leave(sessionId);
            }

            RoomManager::Instance().UnregisterPlayer(sessionId);
        }

        RoomManager::Instance().EnterLobby(sessionId); // Using sessionId overload

        Protocol::S_LeaveRoom res;
        res.set_success(true);
        // Send Response
        S_LeaveRoomPacket respPacket(res);
        ctx.Send(respPacket);
        LOG_INFO("Session {} Left Room -> Lobby", sessionId);
    }
}

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
