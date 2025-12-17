#pragma once
#include "System/ISession.h"
#include <string>
#include "Entity/GameObject.h"

namespace SimpleGame {

class Player : public GameObject
{
public:
    Player(int32_t gameId, System::ISession *session) 
        : GameObject(gameId, Protocol::ObjectType::PLAYER), _session(session)
    {
        // Default Stats
        _maxHp = 100;
        _hp = _maxHp;
    }

    // Default constructor for pooling
    Player() : GameObject(0, Protocol::ObjectType::PLAYER), _session(nullptr) {}

    // Init for pooling
    void Initialize(int32_t gameId, System::ISession *session) {
        _id = gameId;
        _session = session;
        _maxHp = 100;
        _hp = _maxHp;
        _classId = 0;
        _name.clear();
        _vx = _vy = 0;
        _x = _y = 0;
    }

    // Reset for pooling
    void Reset() {
        _session = nullptr;
        _name.clear();
        _classId = 0;
        _hp = 0;
        _maxHp = 0;
        _id = 0;
    }

    uint64_t GetSessionId() const
    {
        return _session ? _session->GetId() : 0;
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

    // Class system?
    void SetClassId(int32_t classId) { _classId = classId; }
    int32_t GetClassId() const { return _classId; }

    void ApplySkills(const std::vector<std::pair<int, int>>& skills) {
        // Example: Skill ID 101 = Max HP
        for (const auto& skill : skills) {
            int id = skill.first;
            int lvl = skill.second;
            
            if (id == 101) {
                // +10 HP per level
                _maxHp += 10 * lvl;
                _hp = _maxHp;
            }
            // Add more skills here
        }
    }

private:
    System::ISession *_session = nullptr;
    std::string _name;
    int32_t _classId = 0;
};

} // namespace SimpleGame
