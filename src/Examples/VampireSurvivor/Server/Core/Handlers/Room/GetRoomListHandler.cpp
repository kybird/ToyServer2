#include "GetRoomListHandler.h"
#include "Protocol/game.pb.h"
#include "GamePackets.h"
#include "Game/RoomManager.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Room {

void GetRoomListHandler::Handle(System::ISession* session, System::PacketView packet)
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
        }

        S_RoomListPacket respPacket(res);
        session->SendPacket(respPacket);
        LOG_INFO("Sent room list to session {}: {} rooms", session->GetId(), res.rooms_size());
    }
}

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
