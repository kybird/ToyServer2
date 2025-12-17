#include "Core/GamePacketHandler.h"
#include "Core/DataManager.h"
#include "Core/LoginController.h"
#include "Game/RoomManager.h"
#include "Core/UserDB.h"
#include "System/Config/Json/JsonConfigLoader.h"
#include "System/Database/DBConnectionPool.h"
#include "System/Database/SQLiteConnection.h"
#include "System/Framework/Framework.h"
#include "System/ILog.h"
#include <iostream>


int main()
{
    // Initialize Logging
    System::GetLog().Init();

    LOG_INFO("SimpleGame Server Starting...");

    // Basic Framework Setup
    System::Framework framework;

    // Packet Handler
    auto packetHandler = std::make_shared<SimpleGame::GamePacketHandler>();

    // Config
    auto config = std::make_shared<System::JsonConfigLoader>();
    if (!config->Load("simple_game_config.json"))
    {
        LOG_ERROR("Failed to load config.");
        return 1;
    }

    // Load Game Data
    if (!SimpleGame::DataManager::Instance().LoadMonsterData("MonsterData.json") ||
        !SimpleGame::DataManager::Instance().LoadWaveData("WaveData.json"))
    {
        LOG_WARN("Failed to load game data (Mocking might be used or files missing).");
    }

    // Init with Config File and Handler
    if (!framework.Init(config, packetHandler))
    {
        LOG_ERROR("Failed to initialize framework.");
        return 1;
    }

    // Initialize DB Pool (SQLite)
System::DBConnectionPool::ConnectionFactory dbFactory = []()
    {
        return new System::SQLiteConnection();
    };

    // Use "game.db" as connection string (filename)
    auto dbPool = std::make_shared<System::DBConnectionPool>(2, "game.db", dbFactory);
    dbPool->Init();

    // Ensure Table Exists
    {
        auto conn = dbPool->Acquire();
        if (conn)
        {
            conn->Execute(
                "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, "
                "password TEXT);"
            );
            // Insert default user if not exists
            conn->Execute("INSERT OR IGNORE INTO users (username, password) VALUES ('test_user', 'password');");
            
            // New Tables for Meta-Progression
            conn->Execute("CREATE TABLE IF NOT EXISTS user_game_data (user_id INTEGER PRIMARY KEY, points INTEGER DEFAULT 0);");
            conn->Execute("CREATE TABLE IF NOT EXISTS user_skills (user_id INTEGER, skill_id INTEGER, level INTEGER, PRIMARY KEY (user_id, skill_id));");

            dbPool->Release(conn);
            LOG_INFO("Database Initialized (game.db).");
        }
        else
        {
            LOG_ERROR("Failed to acquire connection for DB Init.");
        }
    }

    // Initialize UserDB (Persistent Data Access)
    auto userDB = std::make_shared<SimpleGame::UserDB>(dbPool);

    // Initialize Login Controller
    auto loginController = std::make_shared<SimpleGame::LoginController>(dbPool, &framework);
    loginController->Init();

    // Initialize RoomManager
    auto &roomMgr = SimpleGame::RoomManager::Instance();
    roomMgr.TestMethod();
    roomMgr.Init(framework.GetTimer(), userDB);
    
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

    framework.Run();

    return 0;
}
