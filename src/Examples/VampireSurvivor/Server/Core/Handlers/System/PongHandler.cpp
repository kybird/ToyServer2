#include "PongHandler.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"

namespace SimpleGame {
namespace Handlers {
namespace System {

void PongHandler::Handle(::System::SessionContext &ctx, ::System::PacketView packet)
{
    // [Heartbeat] Client responded. Update timeout logic.
    ctx.OnPong();
}

} // namespace System
} // namespace Handlers
} // namespace SimpleGame
