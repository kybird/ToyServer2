#include "ChatHandler.h"
#include "Core/DataManager.h"
#include "Entity/Player.h"
#include "Game/CommandManager.h"
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

void ChatHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    // std::cout << "[DEBUG] Handling C_CHAT" << std::endl;
    Protocol::C_Chat req;
    if (packet.Parse(req))
    {
        std::string msg = req.msg();
        uint64_t sessionId = ctx.Id();
        // std::cout << "[DEBUG] Chat Msg: " << msg << std::endl;

        // If in Lobby, use MQ for Distributed Global Chat
        if (RoomManager::Instance().IsInLobby(sessionId))
        {
            // Serialize "PlayerID:Msg" to JSON
            nlohmann::json j;
            j["p"] = (int32_t)sessionId;
            j["m"] = msg;

            // Publish to MQ (Reliable)
            System::MQ::MessageSystem::Instance().Publish("LobbyChat", j.dump(), System::MQ::MessageQoS::Reliable);
        }
        else
        {
            auto player = RoomManager::Instance().GetPlayer(sessionId);
            if (!player)
                return;

            auto room = RoomManager::Instance().GetRoom(player->GetRoomId());

            // Handle Console Commands
            if (msg.find("/") == 0)
            {
                CommandContext cmdCtx;
                cmdCtx.sessionId = sessionId;
                cmdCtx.roomId = player->GetRoomId();
                cmdCtx.isGM = true; // [Plan] 향후 세션 정보 등에 따라 권한 부여 로직 추가 가능

                CommandManager::Instance().Execute(cmdCtx, msg);

                // Command processing feedback (Optional, CommandManager inside could also send this)
                Protocol::S_Chat res;
                res.set_player_id(0); // System ID 0
                res.set_msg("Command processed: " + msg);
                S_ChatPacket resPacket(res);
                ctx.Send(resPacket);
                return;
            }

            // In Room - Echo local only (or implement Room Chat later)
            Protocol::S_Chat res;
            res.set_player_id((int32_t)sessionId);
            res.set_msg(msg);
            S_ChatPacket chatPacket(res);

            // Simple echo for now
            ctx.Send(chatPacket);
        }
    }
}

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
