#pragma once
#include "Core/UserDB.h"
#include "System/IConfig.h"
#include "System/IDatabase.h"
#include "System/IFramework.h"
#include <memory>


namespace SimpleGame {

class GamePacketHandler;
class LoginController;

class ServerApp
{
public:
    ServerApp();
    ~ServerApp();

    bool Init();
    void Run();

private:
    bool LoadConfig();
    bool InitGameData();
    bool InitFramework();
    bool InitDatabase();
    bool InitGameLogic();
    void RegisterConsoleCommands();

private:
    std::shared_ptr<System::IFramework> _framework;
    std::shared_ptr<System::IConfig> _config;
    std::shared_ptr<System::IDatabase> _db;
    std::shared_ptr<System::ThreadPool> _dbThreadPool;

    std::shared_ptr<GamePacketHandler> _packetHandler;
    std::shared_ptr<UserDB> _userDB;
    std::shared_ptr<LoginController> _loginController;
};

} // namespace SimpleGame
