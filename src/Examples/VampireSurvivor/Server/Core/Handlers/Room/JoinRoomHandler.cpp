#include "JoinRoomHandler.h"
#include "Protocol/game.pb.h"
#include "GamePackets.h"
#include "Game/RoomManager.h"
#include "Entity/PlayerFactory.h"
#include "System/ILog.h"
#include <string>

namespace SimpleGame {
namespace Handlers {
namespace Room {

void JoinRoomHandler::Handle(System::ISession* session, System::PacketView packet)
{
    Protocol::C_JoinRoom req;
    if (packet.Parse(req))
    {
        // 1. Get Room
        int roomId = req.room_id();
        auto room = RoomManager::Instance().GetRoom(roomId);
        if (room)
        {
            // Check if already in room
            if (RoomManager::Instance().GetPlayer(session->GetId()))
            {
                LOG_WARN("Player {} already in room", session->GetId());
                return;
            }

            // 2. Create Player
            int32_t gameId = (int32_t)session->GetId();
            auto player = PlayerFactory::Instance().CreatePlayer(gameId, session);
            player->SetName("Survivor_" + std::to_string(gameId));

            // 3. Send Response FIRST (before entering room)
            Protocol::S_JoinRoom res;
            res.set_success(true);
            res.set_room_id(roomId);

            S_JoinRoomPacket respPacket(res);
            session->SendPacket(respPacket);
            LOG_INFO("Player joined Room {}", roomId);

            // 4. Enter Room (this will send existing objects to the player)
            room->Enter(player);
            // Remove from lobby if there
            RoomManager::Instance().LeaveLobby(session->GetId());
            RoomManager::Instance().RegisterPlayer(session->GetId(), player);
        }
        else
        {
            LOG_WARN("Room {} not found", roomId);
        }
    }
}

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
