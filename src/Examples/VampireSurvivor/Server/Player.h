#pragma once
#include "System/ISession.h"
#include <string>

namespace SimpleGame {

class Player
{
public:
    Player(System::ISession *session) : _session(session)
    {
    }

    uint64_t GetSessionId() const
    {
        return _session->GetId();
    }
    System::ISession *GetSession() const
    {
        return _session;
    }

    void SetName(const std::string &name)
    {
        _name = name;
    }
    const std::string &GetName() const
    {
        return _name;
    }

private:
    System::ISession *_session;
    std::string _name;
};

} // namespace SimpleGame
