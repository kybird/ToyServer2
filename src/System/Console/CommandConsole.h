#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "System/IConfig.h"
#include <memory>

namespace System {

class CommandConsole
{
public:
    CommandConsole(std::shared_ptr<IConfig> config);
    ~CommandConsole();

    // Start the input thread
    void Start();

    // Stop the thread (may block until next input or relies on detach/flag)
    void Stop();

private:
    void InputLoop();
    void ProcessCommand(const std::string &line);

    // Handlers
    void HandleStatus();
    void HandleReload();
    void HandleHelp();
    void HandleQuit();

private:
    std::thread _inputThread;
    std::atomic<bool> _running = false;
    std::shared_ptr<IConfig> _config;
};

} // namespace System
