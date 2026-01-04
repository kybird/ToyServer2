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

    // 1. Check if user exists
    std::string selectQuery = fmt::format("SELECT password FROM users WHERE username = '{}';", evt.username);
    auto res = _db->Query(selectQuery);

    bool success = false;
    bool needsRegistration = false;

    if (res.status.IsOk() && res.value.has_value())
    {
        auto rs = std::move(*res.value);
        if (rs->Next())
        {
            // User exists, verify password
            std::string dbPass = rs->GetString(0);
            if (dbPass == evt.password)
            {
                success = true;
            }
        }
        else
        {
            // User does not exist, trigger auto-registration
            needsRegistration = true;
        }
    }
    else
    {
        // Query failed (e.g., table doesn't exist yet, but in many cases we should just fail)
        LOG_ERROR("Database query failed: {}", res.status.message);
    }

    // 2. Auto-Registration
    if (needsRegistration)
    {
        LOG_INFO("User not found. Registering new user: {}", evt.username);
        std::string insertQuery =
            fmt::format("INSERT INTO users (username, password) VALUES ('{}', '{}');", evt.username, evt.password);
        auto insertStatus = _db->Execute(insertQuery);

        if (insertStatus.IsOk())
        {
            LOG_INFO("Auto-Registration Success: {}", evt.username);
            success = true;
        }
        else
        {
            LOG_ERROR("Auto-Registration Failed for {}: {}", evt.username, insertStatus.message);
        }
    }

    // 3. Send Response
    if (success)
    {
        // Send S_LOGIN Response (Auth Success)
        Protocol::S_Login resMsg;
        resMsg.set_success(true);
        resMsg.set_my_player_id((int32_t)evt.sessionId); // Use SessionID as PlayerID
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
        evt.session->SendPacket(packet);

        LOG_INFO("Login Auth Success: {} (Session: {})", evt.username, evt.sessionId);
    }
    else
    {
        LOG_INFO("Login Failed: {} (Status: {})", evt.username, res.status.message);
    }
}

} // namespace SimpleGame
