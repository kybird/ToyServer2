#include "MoveInputHandler.h"
#include "Game/GameConfig.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"
#include "System/Thread/IStrand.h"

namespace SimpleGame {
namespace Handlers {
namespace Game {

void MoveInputHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    Protocol::C_MoveInput req;
    if (!packet.Parse(req))
        return;
    // Find Player using Global Map
    uint64_t sessionId = ctx.Id();
    auto player = RoomManager::Instance().GetPlayer(sessionId);

    if (!player)
    {
        LOG_WARN("C_MOVE_INPUT from unknown session {}", sessionId);
        return;
    }

    // [Strand] Use Room's Strand for processing
    auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
    if (!room || !room->GetStrand())
    {
        LOG_WARN("Room/Strand not found for C_MOVE_INPUT");
        return;
    }

    // Capture by value for thread safety
    uint32_t clientTick = req.client_tick();
    int32_t dx = req.dir_x();
    int32_t dy = req.dir_y();

    room->GetStrand()->Post(
        [player, clientTick, dx, dy]()
        {
            // LOG_DEBUG("[C_MoveInput] Player={} ClientTick={} Dir=({}, {})", player->GetId(), clientTick, dx, dy);

            player->ApplyInput(clientTick, dx, dy);
        }
    );
}

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
