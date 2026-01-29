#include "ServerApp.h"
#include "Common/GamePackets.h"
#include "Core/DataManager.h"
#include "Core/GamePacketHandler.h"
#include "Core/LoginController.h"
#include "Core/UserDB.h"
#include "Game/RoomManager.h"
#include "Protocol/game.pb.h"
#include "System/Database/DatabaseImpl.h"
#include "System/Framework/Framework.h" // Needed for Framework implementation
#include "System/ICommandConsole.h"
#include "System/ILog.h"
#include "System/MQ/MessageQoS.h"
#include "System/MQ/MessageSystem.h"
#include "System/Session/SessionFactory.h"
#include "System/Thread/ThreadPool.h"

#ifdef USE_SQLITE
#include "System/Drivers/SQLite/SQLiteConnectionFactory.h"
#endif
#ifdef USE_MYSQL
#include "System/Drivers/MySQL/MySQLConnectionFactory.h"
#endif

#include "System/Debug/CrashHandler.h"
#include <iostream>

namespace SimpleGame {

ServerApp::ServerApp()
{
}

ServerApp::~ServerApp()
{
}

bool ServerApp::Init()
{
    // 0. Crash Handler (Hook early)
    System::Debug::CrashHandler::Init();

    // 1. Logging
    System::GetLog().Init();
    LOG_INFO("SimpleGame Server App Initializing...");
    LOG_ERROR("[DEBUG] Build Timestamp: {} {} (Refactored)", __DATE__, __TIME__);

    // 2. Config
    if (!LoadConfig())
        return false;

    // 3. Game Data (Static Data)
    if (!InitGameData())
        return false;

    // 4. Framework (Engine)
    if (!InitFramework())
        return false;

    // 5. Database
    if (!InitDatabase())
        return false;

    // 6. Game Logic (Controllers, Managers)
    if (!InitGameLogic())
        return false;

    // 7. Console Commands
    RegisterConsoleCommands();

    return true;
}

void ServerApp::Run()
{
    if (_framework)
    {
        LOG_INFO("Services Initialized. Running ServerApp...");
        _framework->Run();

        // Safe Shutdown Sequence
        LOG_INFO("ServerApp Stopping...");

        // 1. Cleanup Game Logic first (break cycles)
        RoomManager::Instance().Cleanup();

        // 2. Join Framework (Stop Engine)
        // Note: Join() was added to IFramework interface directly
        _framework->Join();

        LOG_INFO("ServerApp Shutdown Complete.");
    }
}

bool ServerApp::LoadConfig()
{
    _config = System::IConfig::Create();
    if (!_config->Load("data/simple_game_config.json"))
    {
        LOG_ERROR("Failed to load config.");
        return false;
    }
    System::GetLog().SetLogLevel(_config->GetConfig().logLevel);
    return true;
}

bool ServerApp::InitGameData()
{
    bool success = true;
    success &= DataManager::Instance().LoadMonsterData("data/MonsterData.json");
    success &= DataManager::Instance().LoadWaveData("data/WaveData.json");
    success &= DataManager::Instance().LoadPlayerData("data/PlayerData.json");
    success &= DataManager::Instance().LoadSkillData("data/PlayerBaseSkill.json");
    success &= DataManager::Instance().LoadWeaponData("data/WeaponData.json");
    success &= DataManager::Instance().LoadPassiveData("data/PassiveData.json");

    if (!success)
    {
        LOG_WARN("Failed to load some game data. Server may not function correctly.");
    }
    return true;
}

bool ServerApp::InitFramework()
{
    _framework = System::IFramework::Create();
    _packetHandler = std::make_shared<SimpleGame::GamePacketHandler>();

    if (!_framework->Init(_config, _packetHandler))
    {
        LOG_ERROR("Failed to initialize framework.");
        return false;
    }

    // Heartbeat Setup
    System::SessionFactory::SetHeartbeatConfig(
        5000,
        15000,
        [](System::ISession *session)
        {
            Protocol::S_Ping msg;
            msg.set_timestamp(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                )
                    .count()
            );
            SimpleGame::S_PingPacket packet(msg);
            session->SendPacket(packet);
        }
    );

    return true;
}

bool ServerApp::InitDatabase()
{
    // DB Thread Pool
    _dbThreadPool = std::make_shared<System::ThreadPool>(4);
    _dbThreadPool->Start();

    // Factory
    std::unique_ptr<System::IConnectionFactory> dbFactory;
    const auto &cfg = _config->GetConfig();

    bool factoryCreated = false;

    if (cfg.dbType == "mysql")
    {
#ifdef USE_MYSQL
        System::MySQLConfig mysqlCfg;
        mysqlCfg.Host = cfg.dbAddress;
        mysqlCfg.Port = cfg.dbPort;
        mysqlCfg.User = cfg.dbUser;
        mysqlCfg.Password = cfg.dbPassword;
        mysqlCfg.Database = cfg.dbSchema;
        dbFactory = std::make_unique<System::MySQLConnectionFactory>(mysqlCfg);
        factoryCreated = true;
#else
        LOG_ERROR("MySQL not compiled.");
#endif
    }
    else
    {
#ifdef USE_SQLITE
        dbFactory = std::make_unique<System::SQLiteConnectionFactory>();
        factoryCreated = true;
#else
        LOG_ERROR("SQLite not compiled.");
#endif
    }

    if (!factoryCreated)
        return false;

    _db = std::make_shared<System::DatabaseImpl>(
        cfg.dbAddress, cfg.dbWorkerCount, 5000, std::move(dbFactory), _dbThreadPool, _framework->GetDispatcher()
    );

    if (!_db)
        return false;

    _db->Init();

    LOG_INFO("Database Initialized.");
    return true;
}

bool ServerApp::InitGameLogic()
{
    _userDB = std::make_shared<SimpleGame::UserDB>(_db);
    _userDB->InitSchema(); // [New] Initialize Schema here

    _loginController = std::make_shared<SimpleGame::LoginController>(_db, _framework.get());
    _loginController->Init();

    RoomManager::Instance().Init(_framework, _userDB);

    // MQ (Optional)
    auto &mq = System::MQ::MessageSystem::Instance();
    if (!mq.Initialize("nats://localhost:4222", "tcp://localhost:6379", _framework->GetThreadPool().get()))
    {
        LOG_WARN("MQ System skipped (Init failed).");
    }
    else
    {
        LOG_INFO("MQ System Initialized.");
        // Subscribe Logic... (Keep minimal here)
        mq.Subscribe(
            "LobbyChat",
            [](const std::string &, const std::string &msg)
            {
                // Simplification: Keeping original logic short
                try
                {
                    auto json = nlohmann::json::parse(msg);
                    Protocol::S_Chat res;
                    res.set_player_id(json["p"].get<int32_t>());
                    res.set_msg(json["m"].get<std::string>());
                    RoomManager::Instance().BroadcastPacketToLobby(SimpleGame::S_ChatPacket(res));
                } catch (...)
                {
                }
            },
            System::MQ::MessageQoS::Reliable
        );
    }

    return true;
}

void ServerApp::RegisterConsoleCommands()
{
    auto console = _framework->GetCommandConsole();
    if (!console)
        return;

    console->RegisterCommand(
        {"/levelup",
         "Level Up Room 1 Players",
         [](const std::vector<std::string> &args)
         {
             int exp = 100;
             if (!args.empty())
                 try
                 {
                     exp = std::stoi(args[0]);
                 } catch (...)
                 {
                 }
             if (auto r = RoomManager::Instance().GetRoom(1))
                 r->DebugAddExpToAll(exp);
         }}
    );

    console->RegisterCommand(
        {"/spawn",
         "Spawn Monster",
         [](const std::vector<std::string> &args)
         {
             if (args.size() < 2)
                 return;
             try
             {
                 int id = std::stoi(args[0]);
                 int count = std::stoi(args[1]);
                 if (auto r = RoomManager::Instance().GetRoom(1))
                     r->DebugSpawnMonster(id, count);
             } catch (...)
             {
             }
         }}
    );

    console->RegisterCommand(
        {"/god",
         "Toggle God Mode for all players",
         [](const std::vector<std::string> &args)
         {
             if (auto r = RoomManager::Instance().GetRoom(1))
                 r->DebugToggleGodMode();
         }}
    );
}

} // namespace SimpleGame
