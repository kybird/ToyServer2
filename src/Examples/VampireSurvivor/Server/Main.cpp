#include "Core/DataManager.h"
#include "Core/GamePacketHandler.h"
#include "Core/LoginController.h"
#include "Core/UserDB.h"
#include "Game/RoomManager.h"
#include "System/ToyServerSystem.h"
#include <iostream>

int main()
{
    // Initialize Logging
    System::GetLog().Init();

    LOG_INFO("SimpleGame Server Starting...");

    // Basic Framework Setup
    std::shared_ptr<System::IFramework> framework = System::IFramework::Create();

    // Packet Handler
    auto packetHandler = std::make_shared<SimpleGame::GamePacketHandler>();

    // Config
    auto config = System::IConfig::Create();
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
    if (!framework->Init(config, packetHandler))
    {
        LOG_ERROR("Failed to initialize framework.");
        return 1;
    }

    // Initialize Database (SQLite)
    auto db = System::IDatabase::Create("sqlite", "game.db", 2);
    if (!db)
    {
        LOG_ERROR("Failed to create database system.");
        return 1;
    }

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
