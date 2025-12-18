#include "Core/LoginController.h"
#include "Core/GameEvents.h"
#include "Core/Protocol.h"
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
    }

    if (success)
    {
        // Send S_LOGIN Response (Auth Success)
        Protocol::S_Login res;
        res.set_success(true);
        res.set_my_player_id(0); // Not spawned yet
        res.set_map_width(0);
        res.set_map_height(0);

        size_t bodySize = res.ByteSizeLong();
        auto packet = System::PacketUtils::CreatePacket((uint16_t)bodySize);
        if (packet)
        {
            PacketHeader *header = (PacketHeader *)packet->Payload();
            header->size = (uint16_t)(sizeof(PacketHeader) + bodySize);
            header->id = PacketID::S_LOGIN;
            res.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)bodySize);

            evt.session->Send(packet);
            System::PacketUtils::ReleasePacket(packet);
        }

        LOG_INFO("Login Auth Success: {} (Session: {})", evt.username, evt.sessionId);
    }
    else
    {
        LOG_INFO("Login Failed: {} (Status: {})", evt.username, res.status.message);
    }
}

} // namespace SimpleGame
