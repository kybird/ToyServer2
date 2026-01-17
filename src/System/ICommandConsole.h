#pragma once

#include <functional>
#include <string>
#include <vector>

namespace System {

struct CommandDescriptor
{
    std::string command;
    std::string description;
    std::function<void(const std::vector<std::string> &)> handler;
};

class ICommandConsole
{
public:
    virtual ~ICommandConsole() = default;

    // Registers a new command.
    // If the command already exists, it may be overwritten or ignored based on implementation policy.
    virtual void RegisterCommand(const CommandDescriptor &desc) = 0;

    // Unregisters an existing command.
    virtual void UnregisterCommand(const std::string &command) = 0;
};

} // namespace System
