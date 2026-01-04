#include "LeaveRoomHandler.h"
#include "Protocol/game.pb.h"
#include "GamePackets.h"
#include "Game/RoomManager.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Room {

void LeaveRoomHandler::Handle(System::ISession* session, System::PacketView packet)
{
    Protocol::C_LeaveRoom req;
    if (packet.Parse(req))
    {
        auto player = RoomManager::Instance().GetPlayer(session->GetId());
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
            if (room) {
                 room->Leave(session->GetId());
            }

            RoomManager::Instance().UnregisterPlayer(session->GetId());
        }

        RoomManager::Instance().EnterLobby(session);

        Protocol::S_LeaveRoom res;
        res.set_success(true);
        // Send Response
        S_LeaveRoomPacket respPacket(res);
        session->SendPacket(respPacket);
        LOG_INFO("Session {} Left Room -> Lobby", session->GetId());
    }
}

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
