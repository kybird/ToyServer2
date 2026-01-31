#include "System/Console/CommandConsole.h"
#include "System/IConfig.h"
#include "System/ILog.h"
#include "System/Metrics/MetricsCollector.h"
#include "System/Pch.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace System {

CommandConsole::CommandConsole(std::shared_ptr<IConfig> config) : _config(config)
{
    // Register Default Commands

    // 1. /status
    RegisterCommand(
        {"/status",
         "Show server status",
         [this](const std::vector<std::string> &args)
         {
             // Need access to Server Stats... using MetricsCollector?
             // MetricsCollector stores metrics, but maybe not active session count directly unless tracked.
             // For now, print what we have.
             LOG_INFO("Config: RateLimit={}, Burst={}", _config->GetConfig().rateLimit, _config->GetConfig().rateBurst);
         }}
    );

    // 2. /reload
    RegisterCommand(
        {"/reload",
         "Reload configuration",
         [this](const std::vector<std::string> &args)
         {
             LOG_INFO("Reloading Config...");
             if (_config->Load("data/vampire_server_config.json"))
             {
                 LOG_INFO("Config Reloaded.");
             }
             else
             {
                 LOG_ERROR("Failed to reload config.");
             }
         }}
    );

    // 3. /help
    RegisterCommand(
        {"/help",
         "List available commands",
         [this](const std::vector<std::string> &args)
         {
             LOG_INFO("Available Commands:");
             for (const auto &[cmd, desc] : _commands)
             {
                 LOG_INFO("  {:<10} - {}", cmd, desc.description);
             }
         }}
    );

    // 4. /quit
    RegisterCommand(
        {"/quit",
         "Shutdown server",
         [](const std::vector<std::string> &args)
         {
             LOG_INFO("Quit command received. Shutting down...");
             std::exit(0);
         }}
    );
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
    _running = false;
    if (_inputThread.joinable())
    {
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

void CommandConsole::RegisterCommand(const CommandDescriptor &desc)
{
    if (_commands.contains(desc.command))
    {
        LOG_WARN("Command '{}' is already registered. Overwriting.", desc.command);
    }
    _commands[desc.command] = desc;
}

void CommandConsole::UnregisterCommand(const std::string &command)
{
    _commands.erase(command);
}

void CommandConsole::ProcessCommand(const std::string &line)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg)
    {
        args.push_back(arg);
    }

    auto it = _commands.find(cmd);
    if (it != _commands.end())
    {
        try
        {
            it->second.handler(args);
        } catch (const std::exception &e)
        {
            LOG_ERROR("Error executing command '{}': {}", cmd, e.what());
        } catch (...)
        {
            LOG_ERROR("Unknown error executing command '{}'", cmd);
        }
    }
    else
    {
        LOG_INFO("Unknown command: {}", cmd);
    }
}

} // namespace System
