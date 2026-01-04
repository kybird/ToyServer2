#include "PingHandler.h"
#include "Protocol/game.pb.h"
#include "GamePackets.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace System {

void PingHandler::Handle(::System::ISession* session, ::System::PacketView packet)
{
    // [Latency Check] Echo timestamp back to client
    Protocol::C_Ping req;
    if (packet.Parse(req))
    {
        Protocol::S_Pong res;
        res.set_timestamp(req.timestamp());
        S_PongPacket respPacket(res);
        session->SendPacket(respPacket);
    }
}

} // namespace System
} // namespace Handlers
} // namespace SimpleGame
