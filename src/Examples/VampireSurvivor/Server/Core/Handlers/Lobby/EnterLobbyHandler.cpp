#include "EnterLobbyHandler.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Lobby {

void EnterLobbyHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    Protocol::C_EnterLobby req;
    if (packet.Parse(req))
    {
        // [Phase 2 TODO] RoomManager::EnterLobby needs to be updated to accept SessionId
        // For now, we use sessionId-based registration
        uint64_t sessionId = ctx.Id();
        RoomManager::Instance().EnterLobby(sessionId);

        Protocol::S_EnterLobby res;
        res.set_success(true);

        // Send Response
        S_EnterLobbyPacket respPacket(res);
        ctx.Send(respPacket);
        LOG_INFO("Session {} Entered Lobby", sessionId);
    }
    else
    {
        LOG_ERROR("Failed to parse C_ENTER_LOBBY (Len: {})", packet.GetLength());
    }
}

} // namespace Lobby
} // namespace Handlers
} // namespace SimpleGame
