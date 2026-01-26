#include "LoginHandler.h"
#include "GameEvents.h"
#include "Protocol/game.pb.h"
#include "System/Events/EventBus.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Auth {

void LoginHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    Protocol::C_Login req;
    if (packet.Parse(req))
    {
        // [Phase 2 TODO] LoginRequestEvent.session should be removed
        // For now, we only use sessionId
        LoginRequestEvent evt;
        evt.sessionId = ctx.Id();
        evt.username = req.username();
        evt.password = req.password();
        System::EventBus::Instance().Publish(evt);
        LOG_INFO("Login Requested: {}", evt.username);
    }
    else
    {
        LOG_ERROR("Failed to parse C_LOGIN");
    }
}

} // namespace Auth
} // namespace Handlers
} // namespace SimpleGame
