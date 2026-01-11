#include "System/Console/CommandConsole.h"
#include "System/IConfig.h"
#include "System/ILog.h"
#include "System/Metrics/MetricsCollector.h"
#include "System/Pch.h"

#include <iostream>
#include <sstream>

namespace System {

CommandConsole::CommandConsole(std::shared_ptr<IConfig> config) : _config(config)
{
}

CommandConsole::~CommandConsole()
{
    Stop();
}

void CommandConsole::Start()
{
    if (_running.exchange(true))
        return;

    _inputThread = std::thread(&CommandConsole::InputLoop, this);
    LOG_INFO("Command Console Started. Type '/help' for commands.");
}

void CommandConsole::Stop()
{
    // stdin blocking makes it hard to stop cleanly without platform hacks (CancelIoEx etc.)
    // For now, we set running to false. The thread might hang on cin until enter is pressed.
    // Given this is a dev tool, we accept this or use Detach if needed.
    _running = false;
    if (_inputThread.joinable())
    {
        // Option: _inputThread.detach();
        // But better to let main shutdown kill it if it refuses to die.
        // We can hint user to press enter.
        _inputThread.detach();
    }
}

void CommandConsole::InputLoop()
{
    std::string line;
    while (_running && std::getline(std::cin, line))
    {
        if (line.empty())
            continue;
        ProcessCommand(line);
    }
}

void CommandConsole::ProcessCommand(const std::string &line)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "/status")
        HandleStatus();
    else if (cmd == "/reload")
        HandleReload();
    else if (cmd == "/help")
        HandleHelp();
    else if (cmd == "/quit")
        HandleQuit();
    else
        LOG_INFO("Unknown command: {}", cmd);
}

void CommandConsole::HandleStatus()
{
    // Need access to Server Stats... using MetricsCollector?
    // MetricsCollector stores metrics, but maybe not active session count directly unless tracked.
    // For now, print what we have.
    size_t sessions = 0; // TODO: Get from SessionPool or Metrics
    LOG_INFO("Config: RateLimit={}, Burst={}", _config->GetConfig().rateLimit, _config->GetConfig().rateBurst);
}

void CommandConsole::HandleReload()
{
    LOG_INFO("Reloading Config...");
    if (_config->Load("data/server_config.json"))
    {
        LOG_INFO("Config Reloaded.");
    }
    else
    {
        LOG_ERROR("Failed to reload config.");
    }
}

void CommandConsole::HandleHelp()
{
    LOG_INFO("Available Commands:");
    LOG_INFO("  /status  - Show server status");
    LOG_INFO("  /reload  - Reload configuration");
    LOG_INFO("  /quit    - Shutdown server");
}

void CommandConsole::HandleQuit()
{
    LOG_INFO("Quit command received. Shutting down...");
    // Trigger Framework Shutdown?
    // We don't have direct access to Framework instance here unless Singleton or passed in.
    // Ideally we set a global flag or callback.
    // Quick hack: exit(0) is brutal.
    // Better: Assuming Framework handles SIGINT, maybe we can simulate it?
    // Or just let Framework poll a flag.
    std::exit(0);
}

} // namespace System
