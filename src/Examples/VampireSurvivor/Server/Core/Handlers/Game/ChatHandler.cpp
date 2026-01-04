#include "ChatHandler.h"
#include "Protocol/game.pb.h"
#include "GamePackets.h"
#include "Game/RoomManager.h"
#include "System/ILog.h"
#include <iostream>
#include <string>

namespace SimpleGame {
namespace Handlers {
namespace Game {

void ChatHandler::Handle(System::ISession* session, System::PacketView packet)
{
    std::cout << "[DEBUG] Handling C_CHAT" << std::endl;
    Protocol::C_Chat req;
    if (packet.Parse(req))
    {
        std::string msg = req.msg();
        std::cout << "[DEBUG] Chat Msg: " << msg << std::endl;

        // Broadcast
        Protocol::S_Chat res;
        res.set_player_id((int32_t)session->GetId());
        res.set_msg(msg);

        S_ChatPacket chatPacket(res);
        // If in Lobby, broadcast to all lobby sessions
        if (RoomManager::Instance().IsInLobby(session->GetId()))
        {
            std::cout << "[DEBUG] Broadcasting to Lobby..." << std::endl;
            RoomManager::Instance().BroadcastPacketToLobby(chatPacket);
            std::cout << "[DEBUG] Broadcast to Lobby Complete." << std::endl;
        }
        else
        {
            // In Room - Broadcast via Room
            std::cout << "[DEBUG] Broadcasting to Room..." << std::endl;
            auto player = RoomManager::Instance().GetPlayer(session->GetId());
            if (player)
            {
                // Room broadcast logic
                // player->GetRoom()->BroadcastPacket(chatPacket);
                LOG_WARN("Room Chat not fully implemented. Echoing only.");
                session->SendPacket(chatPacket);
            }
        }
    }
}

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
