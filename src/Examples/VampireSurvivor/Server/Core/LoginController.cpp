#include "Core/LoginController.h"
#include "Protocol/game.pb.h"
#include "Core/GameEvents.h"
#include "Core/Protocol.h"
#include "System/Database/DBConnectionPool.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Events/EventBus.h"
#include "System/IFramework.h"
#include "System/ILog.h"
#include "System/ISession.h"
#include "System/Network/PacketUtils.h"
#include <fmt/format.h>

namespace SimpleGame {

LoginController::LoginController(std::shared_ptr<System::DBConnectionPool> dbPool, System::IFramework *framework)
    : _dbPool(dbPool), _framework(framework)
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

    auto *conn = _dbPool->Acquire();
    if (conn)
    {
        // Real Verification using SQL
        std::string query = fmt::format("SELECT password FROM users WHERE username = '{}';", evt.username);
        auto rs = conn->Query(query);

        bool success = false;
        if (rs && rs->Next())
        {
            std::string dbPass = rs->GetString(0);
            if (dbPass == evt.password)
            {
                success = true;
            }
        }

        _dbPool->Release(conn);

        if (success)
        {
            // Send S_LOGIN Response (Auth Success)
            Protocol::S_Login res;
            res.set_success(true);
            res.set_my_player_id(0); // Not spawned yet
            res.set_map_width(0);
            res.set_map_height(0);

            size_t bodySize = res.ByteSizeLong();
            auto packet = System::MessagePool::AllocatePacket((uint16_t)bodySize);
            if (packet)
            {
                PacketHeader *header = (PacketHeader *)packet->Payload();
                header->size = (uint16_t)(sizeof(PacketHeader) + bodySize);
                header->id = PacketID::S_LOGIN;
                res.SerializeToArray(packet->Payload() + sizeof(PacketHeader), (int)bodySize);

                evt.session->Send((System::PacketMessage *)packet);
                System::PacketUtils::ReleasePacket(packet);
            }

            LOG_INFO("Login Auth Success: {} (Session: {})", evt.username, evt.sessionId);
        }
        else
        {
            LOG_INFO("Login Failed: {}", evt.username);
        }
    }
    else
    {
        LOG_ERROR("Failed to acquire DB connection for Login.");
    }
}

} // namespace SimpleGame
