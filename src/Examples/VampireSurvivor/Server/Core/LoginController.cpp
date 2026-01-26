#include "Core/LoginController.h"
#include "Core/GameEvents.h"
#include "Game/GameConfig.h"
#include "Game/Room.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol.h"
#include "Protocol/game.pb.h"
#include <fmt/format.h>

namespace SimpleGame {

LoginController::LoginController(std::shared_ptr<System::IDatabase> db, System::IFramework *framework)
    : _db(std::move(db)), _framework(framework)
{
}

void LoginController::Init()
{
    // Subscribe to Logic Queue via Framework
    _framework->Subscribe<LoginRequestEvent>(
        [this](const LoginRequestEvent &evt)
        {
            OnLogin(evt);
        }
    );

    LOG_INFO("LoginController Initialized.");
}

void LoginController::OnLogin(const LoginRequestEvent &evt)
{
    LOG_INFO("Processing Login Request for User: {}", evt.username);

    std::string username = evt.username;
    std::string password = evt.password;
    uint64_t sessionId = evt.sessionId;

    _db->AsyncRunInTransaction(
        [username, password](System::IDatabase *db) -> bool
        {
            // 1. Check if user exists
            std::string selectQuery = fmt::format("SELECT password FROM users WHERE username = '{}';", username);
            auto res = db->Query(selectQuery);

            if (res.status.IsOk() && res.value.has_value())
            {
                auto rs = std::move(*res.value);
                if (rs->Next())
                {
                    // User exists, verify password
                    std::string dbPass = rs->GetString(0);
                    return (dbPass == password);
                }
            }

            // 2. Auto-Registration
            std::string insertQuery =
                fmt::format("INSERT INTO users (username, password) VALUES ('{}', '{}');", username, password);
            auto insertStatus = db->Execute(insertQuery);
            return insertStatus.IsOk();
        },
        [this, username, sessionId](bool success)
        {
            // 3. Send Response (on Main Thread)
            // Auth Success
            Protocol::S_Login resMsg;
            resMsg.set_success(success);

            if (success)
            {
                resMsg.set_my_player_id(static_cast<int32_t>(sessionId));
                resMsg.set_map_width(0);
                resMsg.set_map_height(0);
                resMsg.set_server_tick_rate(GameConfig::TPS);
                resMsg.set_server_tick_interval(GameConfig::TICK_INTERVAL_SEC);

                uint32_t currentTick = 0;
                auto room = RoomManager::Instance().GetRoom(GameConfig::DEFAULT_ROOM_ID);
                if (room != nullptr)
                {
                    currentTick = room->GetServerTick();
                    resMsg.set_server_tick(currentTick);

                    S_LoginPacket packet(resMsg);
                    room->SendToPlayer(sessionId, packet);
                }
                LOG_INFO("Login Auth Success: {} (Session: {})", username, sessionId);
            }
            else
            {
                LOG_INFO("Login Failed: {}", username);
                auto room = RoomManager::Instance().GetRoom(GameConfig::DEFAULT_ROOM_ID);
                if (room != nullptr)
                {
                    S_LoginPacket packet(resMsg);
                    room->SendToPlayer(sessionId, packet);
                }
            }
        }
    );
}

} // namespace SimpleGame
