#include "GameReadyHandler.h"
#include "Protocol/game.pb.h"
#include "Game/RoomManager.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Game {

void GameReadyHandler::Handle(System::ISession* session, System::PacketView packet)
{
    // [Game Ready] Client finished loading, send spawn packets
    auto player = RoomManager::Instance().GetPlayer(session->GetId());
    if (player)
    {
        auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
        if (room)
        {
            room->OnPlayerReady(session->GetId());
        }
    }
}

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
