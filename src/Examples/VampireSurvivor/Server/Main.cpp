#include "Common/GamePackets.h"
#include "Core/DataManager.h"
#include "Core/GamePacketHandler.h"
#include "Core/LoginController.h"
#include "Core/UserDB.h"
#include "Game/RoomManager.h"
#include "Protocol/game.pb.h"
#include "System/Database/DatabaseImpl.h"

#ifdef USE_SQLITE
#include "System/Drivers/SQLite/SQLiteConnectionFactory.h"
#endif

#ifdef USE_MYSQL
#include "System/Drivers/MySQL/MySQLConnectionFactory.h"
#endif

#include "System/MQ/MessageQoS.h"
#include "System/MQ/MessageSystem.h"

#include "System/ICommandConsole.h"
#include "System/Session/SessionFactory.h"
#include "System/Thread/ThreadPool.h"
#include "System/ToyServerSystem.h"
#include <iostream>

int main()
{
    // Initialize Logging
    System::GetLog().Init();

    LOG_INFO("SimpleGame Server Starting...");
    LOG_ERROR("[DEBUG] Build Timestamp: {} {} (Fresh Build)", __DATE__, __TIME__);
    // Force Recompile

    // Basic Framework Setup
    std::shared_ptr<System::IFramework> framework = System::IFramework::Create();

    // Packet Handler
    auto packetHandler = std::make_shared<SimpleGame::GamePacketHandler>();

    // Config
    auto config = System::IConfig::Create();
    if (!config->Load("data/simple_game_config.json"))
    {
        LOG_ERROR("Failed to load config.");
        return 1;
    }

    // Set Log Level from Config
    System::GetLog().SetLogLevel(config->GetConfig().logLevel);

    // Load Game Data
    if (!SimpleGame::DataManager::Instance().LoadMonsterData("data/MonsterData.json") ||
        !SimpleGame::DataManager::Instance().LoadWaveData("data/WaveData.json") ||
        !SimpleGame::DataManager::Instance().LoadPlayerData("data/PlayerData.json") ||
        !SimpleGame::DataManager::Instance().LoadSkillData("data/PlayerBaseSkill.json") ||
        !SimpleGame::DataManager::Instance().LoadWeaponData("data/WeaponData.json") ||
        !SimpleGame::DataManager::Instance().LoadPassiveData("data/PassiveData.json"))
    {
        LOG_WARN("Failed to load game data. Server may not function correctly without data files in data/ directory.");
    }

    // Init with Config File and Handler
    if (!framework->Init(config, packetHandler))
    {
        LOG_ERROR("Failed to initialize framework.");
        return 1;
    }

    // [Command Console Registration]
    auto console = framework->GetCommandConsole();
    if (console)
    {
        console->RegisterCommand(
            {"/levelup",
             "Level Up all players in Room 1. Usage: /levelup [amount]",
             [](const std::vector<std::string> &args)
             {
                 int exp = 100; // Default
                 if (!args.empty())
                 {
                     try
                     {
                         exp = std::stoi(args[0]);
                     } catch (...)
                     {
                         LOG_WARN("Invalid exp amount, using default 100");
                     }
                 }

                 auto room = SimpleGame::RoomManager::Instance().GetRoom(1);
                 if (room)
                 {
                     room->DebugAddExpToAll(exp);
                     LOG_INFO("Executed /levelup with {} EXP", exp);
                 }
                 else
                 {
                     LOG_WARN("Room 1 not found for /levelup");
                 }
             }}
        );

        console->RegisterCommand(
            {"/spawn",
             "Spawn Monster [id] [count]",
             [](const std::vector<std::string> &args)
             {
                 if (args.size() < 2)
                 {
                     LOG_WARN("Usage: /spawn [id] [count]");
                     return;
                 }
                 try
                 {
                     int id = std::stoi(args[0]);
                     int count = std::stoi(args[1]);
                     auto room = SimpleGame::RoomManager::Instance().GetRoom(1);
                     if (room)
                     {
                         room->DebugSpawnMonster(id, count);
                         LOG_INFO("Executed /spawn {} {}", id, count);
                     }
                     else
                     {
                         LOG_WARN("Room 1 not found for /spawn");
                     }
                 } catch (...)
                 {
                     LOG_ERROR("Invalid arguments for /spawn");
                 }
             }}
        );
    }

    // [Heartbeat] Configure Ping (5s Interval, 15s Timeout)
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

    // Initialize Thread Pool for Database
    auto dbThreadPool = std::make_shared<System::ThreadPool>(4);
    dbThreadPool->Start();

    // Initialize Database with Factory based on Config
    std::unique_ptr<System::IConnectionFactory> dbFactory;
    const auto &cfg = config->GetConfig();

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
        LOG_INFO("Using MySQL Database Driver. Host: {}, DB: {}", mysqlCfg.Host, mysqlCfg.Database);
#else
        LOG_ERROR("MySQL Driver is not available in this build.");
        return 1;
#endif
    }
    else
    {
#ifdef USE_SQLITE
        // Default SQLite
        dbFactory = std::make_unique<System::SQLiteConnectionFactory>();
        LOG_INFO("Using SQLite Database Driver. File: {}", cfg.dbAddress);
#else
        LOG_ERROR("SQLite Driver is not available in this build.");
        return 1;
#endif
    }

    auto db = std::make_shared<System::DatabaseImpl>(
        cfg.dbAddress, cfg.dbWorkerCount, 5000, std::move(dbFactory), dbThreadPool, framework->GetDispatcher()
    );

    if (!db)
    {
        LOG_ERROR("Failed to create database system.");
        return 1;
    }

    // Initialize DB Connection
    db->Init();

    // Ensure Tables Exist
    {
        db->Execute(
            "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, password "
            "TEXT);"
        );
        db->Execute("INSERT OR IGNORE INTO users (username, password) VALUES ('test_user', 'password');");
        db->Execute(
            "CREATE TABLE IF NOT EXISTS user_game_data (user_id INTEGER PRIMARY KEY, points INTEGER DEFAULT 0);"
        );
        db->Execute(
            "CREATE TABLE IF NOT EXISTS user_skills (user_id INTEGER, skill_id INTEGER, level INTEGER, PRIMARY KEY "
            "(user_id, skill_id));"
        );
        LOG_INFO("Database Initialized (game.db).");
    }

    // Initialize UserDB (Persistent Data Access)
    auto userDB = std::make_shared<SimpleGame::UserDB>(db);

    // Initialize Login Controller
    auto loginController = std::make_shared<SimpleGame::LoginController>(db, framework.get());
    loginController->Init();

    // Initialize RoomManager
    auto &roomMgr = SimpleGame::RoomManager::Instance();
    roomMgr.TestMethod();
    roomMgr.Init(framework, userDB);

    // Initialize MQ System
    auto &mq = System::MQ::MessageSystem::Instance();
    // Assuming local NATS and Redis for now (config could be better)
    if (!mq.Initialize("nats://localhost:4222", "tcp://localhost:6379", framework->GetThreadPool().get()))
    {
        LOG_WARN("MQ System failed to connect. Distributed features may not work.");
    }
    else
    {
        LOG_INFO("MQ System Initialized.");

        // Subscribe to "LobbyChat" on Reliable Channel (Redis)
        mq.Subscribe(
            "LobbyChat",
            [](const std::string &topic, const std::string &msg)
            {
                // This callback runs on a background thread (RedisStreamDriver poll thread)
                // We need to parse the message.
                // But wait, the msg is just the payload string?
                // ChatHandler sends "json" or "protobuf"?
                // Let's assume ChatHandler sends RAW STRING for msg based on previous analysis of ChatHandler.
                // Oh wait, `ChatHandler` extracts `req.msg()` which is string.
                // But we need PlayerID to construct S_Chat packet.
                // We should define a simple JSON format or delimiters for MQ payload: "PlayerID:Message"

                // For now, let's look at `S_ChatPacket`. It takes `Protocol::S_Chat`.
                // `S_Chat` has `player_id` and `msg`.
                // So we need to serialize both. JSON is easiest.

                try
                {
                    auto json = nlohmann::json::parse(msg);
                    int32_t playerId = json["p"];
                    std::string chatMsg = json["m"];

                    Protocol::S_Chat res;
                    res.set_player_id(playerId);
                    res.set_msg(chatMsg);
                    SimpleGame::S_ChatPacket pkt(res);

                    // Broadcast to Local Lobby Users
                    SimpleGame::RoomManager::Instance().BroadcastPacketToLobby(pkt);

                } catch (...)
                {
                    LOG_ERROR("Failed to parse LobbyChat MQ message: {}", msg);
                }
            },
            System::MQ::MessageQoS::Reliable
        );
    }

    // Default room 1 created by ctor -> Re-created/Available
    auto room = roomMgr.GetRoom(1);
    if (room)
    {
        LOG_INFO("Default Room 1 confirmed available.");
    }
    else
    {
        LOG_ERROR("Default Room 1 Missing!");
    }

    // Verification Logic (Simulate network packet)
    LOG_INFO("Services Initialized. Running...");

    framework->Run();

    return 0;
}
