#include "GameReadyHandler.h"
#include "Game/RoomManager.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Game {

void GameReadyHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    // [Game Ready] Client finished loading, send spawn packets
    uint64_t sessionId = ctx.Id();
    auto player = RoomManager::Instance().GetPlayer(sessionId);
    if (player)
    {
        auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
        if (room)
        {
            room->OnPlayerReady(sessionId);
        }
    }
}

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
