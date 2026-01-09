#pragma once
#include "GameEvents.h"
#include "System/ToyServerSystem.h"
#include <memory>

namespace SimpleGame {

class LoginController
{
public:
    LoginController(std::shared_ptr<System::IAsyncDatabase> db, System::IFramework *framework);

    void Init();

private:
    void OnLogin(const LoginRequestEvent &evt);

    std::shared_ptr<System::IAsyncDatabase> _db;
    System::IFramework *_framework;
};

} // namespace SimpleGame
