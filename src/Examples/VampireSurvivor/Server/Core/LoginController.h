#pragma once

#include "GameEvents.h"
#include "System/Database/DBConnectionPool.h"
#include "System/Events/EventBus.h"
#include "System/IFramework.h"
#include "System/ILog.h"
#include <fmt/format.h> // For SQL formatting


using namespace System;

namespace SimpleGame {

class LoginController
{
public:
    LoginController(std::shared_ptr<DBConnectionPool> dbPool, System::IFramework *framework)
        : _dbPool(dbPool), _framework(framework)
    {
    }

    void Init()
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

private:
    void OnLogin(const LoginRequestEvent &evt)
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
                LOG_INFO("Login Success: {}", evt.username);
                // Send Response
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

    std::shared_ptr<DBConnectionPool> _dbPool;
    System::IFramework *_framework;
};

} // namespace SimpleGame
