#include "LoginHandler.h"
#include "Protocol/game.pb.h"
#include "GameEvents.h"
#include "System/Events/EventBus.h"
#include "System/ILog.h"

namespace SimpleGame {
namespace Handlers {
namespace Auth {

void LoginHandler::Handle(System::ISession* session, System::PacketView packet)
{
    Protocol::C_Login req;
    if (packet.Parse(req))
    {
        // Map to Event
        LoginRequestEvent evt;
        evt.sessionId = session->GetId();
        evt.session = session;
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
