#include "PingHandler.h"
#include "Game/RoomManager.h" // [New]
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace System {

void PingHandler::Handle(::System::SessionContext &ctx, ::System::PacketView packet)
{
    // [Latency Check] Echo timestamp back to client
    Protocol::C_Ping req;
    if (packet.Parse(req))
    {
        // [Fix] Zombie Client Detection
        // If a client is sending C_PING but is not known to RoomManager (neither in Room nor Lobby),
        // it means it's a stale connection (e.g. from before server restart) that hasn't re-logged in.
        // We must Force Disconnect to tell the client to reset/re-login.
        uint64_t sessionId = ctx.Id();
        bool isKnown =
            RoomManager::Instance().GetPlayer(sessionId) != nullptr || RoomManager::Instance().IsInLobby(sessionId);

        if (!isKnown)
        {
            LOG_WARN("Session {} sent C_PING but is not Logged In. Closing connection.", sessionId);
            ctx.Close();
            return;
        }

        Protocol::S_Pong res;
        res.set_timestamp(req.timestamp());
        S_PongPacket respPacket(res);
        ctx.Send(respPacket);
    }
}

} // namespace System
} // namespace Handlers
} // namespace SimpleGame
