#include "PongHandler.h"
#include "Protocol/game.pb.h"
#include "GamePackets.h"

namespace SimpleGame {
namespace Handlers {
namespace System {

void PongHandler::Handle(::System::ISession* session, ::System::PacketView packet)
{
    // [Heartbeat] Client responded. Update timeout logic.
    session->OnPong();
}

} // namespace System
} // namespace Handlers
} // namespace SimpleGame
