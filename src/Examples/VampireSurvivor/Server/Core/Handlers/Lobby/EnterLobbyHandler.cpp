#include "EnterLobbyHandler.h"
#include "Protocol/game.pb.h"
#include "GamePackets.h"
#include "Game/RoomManager.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Lobby {

void EnterLobbyHandler::Handle(System::ISession* session, System::PacketView packet)
{
    Protocol::C_EnterLobby req;
    if (packet.Parse(req))
    {
        RoomManager::Instance().EnterLobby(session); // Now passes session pointer

        Protocol::S_EnterLobby res;
        res.set_success(true);

        // Send Response
        S_EnterLobbyPacket respPacket(res);
        session->SendPacket(respPacket);
        LOG_INFO("Session {} Entered Lobby", session->GetId());
    }
    else
    {
        LOG_ERROR("Failed to parse C_ENTER_LOBBY (Len: {})", packet.GetLength());
    }
}

} // namespace Lobby
} // namespace Handlers
} // namespace SimpleGame
