#pragma once

#include <atomic>
#include <map>
#include <string>
#include <thread>

#include "System/ICommandConsole.h"
#include "System/IConfig.h"
#include <memory>

namespace System {

class CommandConsole : public ICommandConsole
{
public:
    CommandConsole(std::shared_ptr<IConfig> config);
    ~CommandConsole() override;

    // Start the input thread
    // Start the input thread
    void Start();

    // Stop the thread (may block until next input or relies on detach/flag)
    void Stop();

    // ICommandConsole Implementation
    void RegisterCommand(const CommandDescriptor &desc) override;
    void UnregisterCommand(const std::string &command) override;

    // Test/Internal: Execute a single command line
    void ProcessCommand(const std::string &line);

private:
    void InputLoop();

    std::thread _inputThread;
    std::atomic<bool> _running = false;
    std::shared_ptr<IConfig> _config;

    std::map<std::string, CommandDescriptor> _commands;
};

} // namespace System
