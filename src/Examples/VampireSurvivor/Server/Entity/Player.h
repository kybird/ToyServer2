#pragma once
#include "Entity/GameObject.h"
#include "Math/Vector2.h"
#include "System/ILog.h"
#include "System/ISession.h"
#include <string>

namespace SimpleGame {

class Player : public GameObject
{
public:
    Player(int32_t gameId, System::ISession *session)
        : GameObject(gameId, Protocol::ObjectType::PLAYER), _session(session)
    {
    }

    // Default constructor for pooling
    Player() : GameObject(0, Protocol::ObjectType::PLAYER), _session(nullptr)
    {
    }

    // Init for pooling
    void Initialize(int32_t gameId, System::ISession *session, int32_t hp, float speed)
    {
        _id = gameId;
        _session = session;
        _maxHp = hp;
        _hp = hp;
        _radius = 0.2f; // Lag Compensation: Visual(0.5) - BufferDelay
        _speed = speed;
        _classId = 0;
        _name.clear();
        _vx = _vy = 0;
        _x = _y = 0;
        _lastInputTick = 0;
        _exp = 0;
        _maxExp = 100;
        _level = 1;
    }

    void ApplyInput(uint32_t clientTick, int32_t dx, int32_t dy)
    {
        _lastInputTick = clientTick;

        Vector2 dir(static_cast<float>(dx), static_cast<float>(dy));

        if (dir.IsZero())
        {
            SetVelocity(0, 0);
            SetState(Protocol::IDLE);
            return;
        }

        dir.Normalize();
        _facingDirX = dir.x;
        _facingDirY = dir.y;

        SetVelocity(dir.x * _speed, dir.y * _speed);
        SetState(Protocol::MOVING);
        LOG_DEBUG(
            "[SetInput] Player={} ClientTick={} Dir=({:.2f}, {:.2f}) Vel=({:.2f},{:.2f})",
            GetId(),
            clientTick,
            dir.x,
            dir.y,
            GetVX(),
            GetVY()
        );
    }

    void TakeDamage(int32_t damage, Room *room) override
    {
        if (IsDead())
            return;

        _hp -= damage;
        if (_hp <= 0)
        {
            _hp = 0;
            SetState(Protocol::ObjectState::DEAD);
        }
    }

    bool IsDead() const
    {
        return _state == Protocol::ObjectState::DEAD;
    }

    // Reset for pooling
    void Reset()
    {
        _session = nullptr;
        _name.clear();
        _classId = 0;
        _hp = 0;
        _maxHp = 0;
        _id = 0;
        _state = Protocol::ObjectState::IDLE;
        _facingDirX = 1.0f;
        _facingDirY = 0.0f;
        _exp = 0;
        _maxExp = 100;
        _level = 1;
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
    void SetClassId(int32_t classId)
    {
        _classId = classId;
    }
    int32_t GetClassId() const
    {
        return _classId;
    }

    void SetRoomId(int roomId)
    {
        _currentRoomId = roomId;
    }
    int GetRoomId() const
    {
        return _currentRoomId;
    }

    Vector2 GetFacingDirection() const
    {
        return Vector2(_facingDirX, _facingDirY);
    }

    void ApplySkills(const std::vector<std::pair<int, int>> &skills)
    {
        // Example: Skill ID 101 = Max HP
        for (const auto &skill : skills)
        {
            int id = skill.first;
            int lvl = skill.second;

            if (id == 101)
            {
                // +10 HP per level
                _maxHp += 10 * lvl;
                _hp = _maxHp;
            }
            // Add more skills here
        }
    }

    uint32_t GetLastProcessedClientTick() const
    {
        return _lastInputTick;
    }

    // Experience & Level
    int32_t GetExp() const
    {
        return _exp;
    }
    int32_t GetMaxExp() const
    {
        return _maxExp;
    }
    int32_t GetLevel() const
    {
        return _level;
    }

    void AddExp(int32_t amount, Room *room);

private:
    System::ISession *_session = nullptr;
    std::string _name;
    int32_t _classId = 0;
    int _currentRoomId = 0;
    float _speed = 5.f;
    uint32_t _lastInputTick = 0;
    float _facingDirX = 1.0f;
    float _facingDirY = 0.0f;

    // Experience & Level
    int32_t _exp = 0;
    int32_t _maxExp = 100;
    int32_t _level = 1;
};

} // namespace SimpleGame
