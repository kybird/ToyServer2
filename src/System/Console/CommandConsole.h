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
private:
    // Shared state to prevent UAF when thread is detached and CommandConsole is destroyed
    struct SharedState
    {
        std::atomic<bool> _running{false};
        std::map<std::string, CommandDescriptor> _commands;
    };

public:
    CommandConsole(std::shared_ptr<IConfig> config);
    ~CommandConsole() override;

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
    std::thread _inputThread;
    std::shared_ptr<IConfig> _config;
    std::shared_ptr<SharedState> _state;
};

} // namespace System
