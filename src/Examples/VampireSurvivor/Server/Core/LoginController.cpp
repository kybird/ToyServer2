#include "Core/LoginController.h"
#include "Core/GameEvents.h"
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

    // Real Verification using SQL
    std::string query = fmt::format("SELECT password FROM users WHERE username = '{}';", evt.username);
    auto res = _db->Query(query);

    bool success = false;
    bool isNewUser = false;

    if (res.status.IsOk() && res.value.has_value())
    {
        auto rs = std::move(*res.value); // 소유권 이전 (RAII)
        if (rs->Next())
        {
            std::string dbPass = rs->GetString(0);
            if (dbPass == evt.password)
            {
                success = true;
            }
        }
        else
        {
            // User not found, try auto-register
            LOG_INFO("User not found: {}. Attempting auto-registration...", evt.username);
            std::string insertQuery =
                fmt::format("INSERT INTO users (username, password) VALUES ('{}', '{}');", evt.username, evt.password);
            auto insertRes = _db->Execute(insertQuery);
            if (insertRes.IsOk())
            {
                LOG_INFO("Auto-registration success for user: {}", evt.username);
                success = true;
                isNewUser = true;
            }
            else
            {
                LOG_ERROR("Auto-registration failed for user: {}: {}", evt.username, insertRes.message);
            }
        }
    }

    if (success)
    {
        // Send S_LOGIN Response (Auth Success)
        Protocol::S_Login res;
        res.set_success(true);
        res.set_my_player_id(0); // Not spawned yet
        res.set_map_width(0);
        res.set_map_height(0);

        S_LoginPacket packet(res);
        evt.session->SendPacket(packet);

        if (isNewUser)
        {
            LOG_INFO("New User Login (Auto-Registered): {} (Session: {})", evt.username, evt.sessionId);
        }
        else
        {
            LOG_INFO("Login Auth Success: {} (Session: {})", evt.username, evt.sessionId);
        }
    }
    else
    {
        LOG_INFO("Login Failed: {} (Status: {})", evt.username, res.status.message);
    }
}

} // namespace SimpleGame
