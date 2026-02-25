#include "GetRoomListHandler.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Room {

void GetRoomListHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    Protocol::C_GetRoomList req;
    if (packet.Parse(req))
    {
        auto rooms = RoomManager::Instance().GetAllRooms();

        Protocol::S_RoomList res;
        for (const auto &room : rooms)
        {
            // Filter joinable
            if (req.only_joinable() && room->IsPlaying())
                continue;

            auto *roomInfo = res.add_rooms();
            roomInfo->set_room_id(room->GetId());
            roomInfo->set_current_players(room->GetPlayerCount());
            roomInfo->set_max_players(room->GetMaxPlayers());
            roomInfo->set_is_playing(room->IsPlaying());
            roomInfo->set_room_title(room->GetTitle());
            roomInfo->set_map_id(room->GetMapId());
        }

        S_RoomListPacket respPacket(res);
        ctx.Send(respPacket);
        LOG_INFO("Sent room list to session {}: {} rooms", ctx.Id(), res.rooms_size());
    }
}

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
