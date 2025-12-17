#pragma once

#include "GameEvents.h"
#include "System/IFramework.h"
#include <memory>


// 임시. 나중에 싹고쳐야함
namespace System {
    class DBConnectionPool;
}

namespace SimpleGame {

class LoginController
{
public:
    LoginController(std::shared_ptr<System::DBConnectionPool> dbPool, System::IFramework *framework);

    void Init();

private:
    void OnLogin(const LoginRequestEvent &evt);

    std::shared_ptr<System::DBConnectionPool> _dbPool;
    System::IFramework *_framework;
};

} // namespace SimpleGame
