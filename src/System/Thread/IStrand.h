#pragma once
#include <functional>

namespace System {

class IStrand
{
public:
    virtual ~IStrand() = default;

    // Dispatch a task to be executed sequentially in this strand's context
    virtual void Post(std::function<void()> task) = 0;
};

} // namespace System
