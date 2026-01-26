#include "Handlers/Game/SelectLevelUpHandler.h"
#include "Entity/Player.h"
#include "Game/LevelUpManager.h"
#include "Game/Room.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "System/ILog.h"
#include "System/Thread/IStrand.h"

namespace SimpleGame {
namespace Handlers {
namespace Game {

void SelectLevelUpHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    Protocol::C_SelectLevelUp req;
    uint64_t sessionId = ctx.Id();
    if (!packet.Parse(req))
    {
        LOG_ERROR("[SelectLevelUpHandler] Failed to parse C_SELECT_LEVEL_UP from session {}", sessionId);
        return;
    }

    auto player = RoomManager::Instance().GetPlayer(sessionId);
    if (!player)
    {
        LOG_WARN("[SelectLevelUpHandler] Player not found for session {}", sessionId);
        return;
    }

    int roomId = player->GetRoomId();
    auto room = RoomManager::Instance().GetRoom(roomId);
    if (!room || !room->GetStrand())
    {
        LOG_WARN("[SelectLevelUpHandler] Room/Strand {} not found for player {}", roomId, player->GetId());
        return;
    }

    // [Strand] Process selection in Room's strand for thread safety
    int optionIndex = req.option_index();

    room->GetStrand()->Post(
        [player, optionIndex, room]()
        {
            LevelUpManager levelUpMgr;
            levelUpMgr.ApplySelection(player.get(), optionIndex, room.get());

            player->ExitLevelUpState(room.get());

            LOG_INFO(
                "[SelectLevelUpHandler] Processed selection for Player {} Option index {}", player->GetId(), optionIndex
            );
        }
    );
}

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
