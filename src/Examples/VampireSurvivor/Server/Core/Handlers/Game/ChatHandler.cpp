#include "ChatHandler.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"
#include <iostream>
#include <string>

#include "System/MQ/MessageQoS.h"
#include "System/MQ/MessageSystem.h"
#include <nlohmann/json.hpp>

namespace SimpleGame {
namespace Handlers {
namespace Game {

void ChatHandler::Handle(System::ISession *session, System::PacketView packet)
{
    // std::cout << "[DEBUG] Handling C_CHAT" << std::endl;
    Protocol::C_Chat req;
    if (packet.Parse(req))
    {
        std::string msg = req.msg();
        // std::cout << "[DEBUG] Chat Msg: " << msg << std::endl;

        // If in Lobby, use MQ for Distributed Global Chat
        if (RoomManager::Instance().IsInLobby(session->GetId()))
        {
            // Serialize "PlayerID:Msg" to JSON
            nlohmann::json j;
            j["p"] = (int32_t)session->GetId();
            j["m"] = msg;

            // Publish to MQ (Reliable)
            System::MQ::MessageSystem::Instance().Publish("LobbyChat", j.dump(), System::MQ::MessageQoS::Reliable);
        }
        else
        {
            // In Room - Echo local only (or implement Room Chat later)
            Protocol::S_Chat res;
            res.set_player_id((int32_t)session->GetId());
            res.set_msg(msg);
            S_ChatPacket chatPacket(res);

            // std::cout << "[DEBUG] Broadcasting to Room..." << std::endl;
            auto player = RoomManager::Instance().GetPlayer(session->GetId());
            if (player)
            {
                // Simple echo for now
                session->SendPacket(chatPacket);
            }
        }
    }
}

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
