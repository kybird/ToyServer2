#include "ServerApp.h"
#include "Common/GamePackets.h"
#include "Core/DataManager.h"
#include "Core/GamePacketHandler.h"
#include "Core/LoginController.h"
#include "Core/UserDB.h"
#include "Game/CommandManager.h"
#include "Game/Room.h"
#include "Game/RoomManager.h"
#include "Protocol/game.pb.h"
#include "System/Drivers/MySQL/MySQLDriver.h"
#include "System/Drivers/SQLite/SQLiteDriver.h"
#include "System/Framework/Framework.h" // Needed for Framework implementation
#include "System/ICommandConsole.h"
#include "System/ILog.h"
#include "System/MQ/MessageQoS.h"
#include "System/MQ/MessageSystem.h"
#include "System/Session/SessionFactory.h"


#include "System/Debug/CrashHandler.h"
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

        // 1.5 Cleanup Session Pools (Prevent UAF during CRT shutdown)
        System::SessionFactory::Cleanup();

        // 2. Join Framework (Stop Engine)
        // Note: Join() was added to IFramework interface directly
        _framework->Join();

        LOG_INFO("ServerApp Shutdown Complete.");
    }
}

bool ServerApp::LoadConfig()
{
    _config = System::IConfig::Create();
    if (!_config->Load("data/vampire_server_config.json"))
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

    // 타일맵 로드 (실패해도 일단 경고만 남기고 서버 시작)
    if (!DataManager::Instance().LoadMapData(1, "data/Map01.json"))
    {
        LOG_WARN("Failed to load Map01.json");
    }

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
    _db = _framework->GetDatabase();

    if (!_db)
    {
        LOG_ERROR("Database initialization failed (Framework did not provide IDatabase).");
        return false;
    }

    LOG_INFO("Database Instance obtained from Framework.");
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

    // Command Manager
    CommandManager::Instance().Init();

    return true;
}

void ServerApp::RegisterConsoleCommands()
{
    auto console = _framework->GetCommandConsole();
    if (!console)
        return;

    // 통합 CommandManager의 모든 명령어를 터미널 콘솔에 등록
    for (const auto &[cmd, info] : CommandManager::Instance().GetCommands())
    {
        console->RegisterCommand(
            {cmd,
             info.description,
             [cmd](const std::vector<std::string> &args)
             {
                 CommandContext ctx;
                 ctx.sessionId = 0; // Server Console
                 ctx.roomId = 1;    // Default Room 1
                 ctx.isGM = true;

                 // 인자로 들어온 것들을 다시 합쳐서 Execute 호출 (추후 ICommandConsole 인터페이스 개선 가능)
                 std::string line = cmd;
                 for (const auto &arg : args)
                     line += " " + arg;
                 CommandManager::Instance().Execute(ctx, line);
             }}
        );
    }
}

} // namespace SimpleGame
