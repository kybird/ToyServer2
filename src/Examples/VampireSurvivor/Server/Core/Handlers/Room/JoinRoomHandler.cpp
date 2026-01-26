#include "JoinRoomHandler.h"
#include "Entity/PlayerFactory.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"
#include <string>

namespace SimpleGame {
namespace Handlers {
namespace Room {

void JoinRoomHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    Protocol::C_JoinRoom req;
    if (packet.Parse(req))
    {
        // 1. Get Room
        int roomId = req.room_id();
        auto room = RoomManager::Instance().GetRoom(roomId);
        uint64_t sessionId = ctx.Id();

        if (room)
        {
            // Check if already in room
            if (RoomManager::Instance().GetPlayer(sessionId))
            {
                LOG_WARN("Player {} already in room", sessionId);
                return;
            }

            // 2. Create Player
            // [Phase 2] PlayerFactory::CreatePlayer now takes sessionId (uint64_t)
            int32_t gameId = (int32_t)sessionId;
            auto player = PlayerFactory::Instance().CreatePlayer(gameId, sessionId);
            player->SetName("Survivor_" + std::to_string(gameId));

            // 3. Send Response FIRST (before entering room)
            Protocol::S_JoinRoom res;
            res.set_success(true);
            res.set_room_id(roomId);

            S_JoinRoomPacket respPacket(res);
            ctx.Send(respPacket);
            LOG_INFO("Player joined Room {}", roomId);

            // 4. Enter Room (this will send existing objects to the player)
            room->Enter(player);
            // Remove from lobby if there
            RoomManager::Instance().LeaveLobby(sessionId);
            RoomManager::Instance().RegisterPlayer(sessionId, player);
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
