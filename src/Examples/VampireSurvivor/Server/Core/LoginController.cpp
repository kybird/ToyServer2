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
    : _db(db), _framework(framework)
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

    // 0. Manual Session Ref Count for Async Lifetime
    if (evt.session)
    {
        evt.session->IncRef();
    }
    std::shared_ptr<System::ISession> sessionInfo(
        evt.session,
        [](System::ISession *s)
        {
            if (s)
                s->DecRef();
        }
    );

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

            // 2. Auto-Registration (if not found or query empty)
            // Note: If query failed we might want to fail, but auto-reg usually implies 'if not exist'
            // Ideally we check if error was DB_ERROR or just empty result.
            // Simplified: Try Register.
            std::string insertQuery =
                fmt::format("INSERT INTO users (username, password) VALUES ('{}', '{}');", username, password);
            auto insertStatus = db->Execute(insertQuery);
            return insertStatus.IsOk();
        },
        [this, sessionInfo, username, sessionId](bool success)
        {
            // 3. Send Response (on Main Thread)
            if (success && sessionInfo)
            {
                // Send S_LOGIN Response (Auth Success)
                Protocol::S_Login resMsg;
                resMsg.set_success(true);
                resMsg.set_my_player_id((int32_t)sessionId); // Use SessionID as PlayerID
                resMsg.set_map_width(0);
                resMsg.set_map_height(0);
                resMsg.set_server_tick_rate(GameConfig::TPS);
                resMsg.set_server_tick_interval(GameConfig::TICK_INTERVAL_SEC);

                // [Modified] Send current server tick from Default Room (1)
                uint32_t currentTick = 0;
                auto room = RoomManager::Instance().GetRoom(1);
                if (room)
                {
                    currentTick = room->GetServerTick();
                }
                resMsg.set_server_tick(currentTick);

                S_LoginPacket packet(resMsg);
                sessionInfo->SendPacket(packet);

                LOG_INFO("Login Auth Success: {} (Session: {})", username, sessionId);
            }
            else
            {
                LOG_INFO("Login Failed: {}", username);
                if (sessionInfo)
                {
                    // Optional: Send Login Fail Packet
                    Protocol::S_Login resMsg;
                    resMsg.set_success(false);
                    S_LoginPacket packet(resMsg);
                    sessionInfo->SendPacket(packet);
                }
            }
        }
    );
}

} // namespace SimpleGame
