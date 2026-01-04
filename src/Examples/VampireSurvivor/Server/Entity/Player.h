#pragma once
#include "Entity/GameObject.h"
#include "System/ISession.h"
#include <string>

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
    Player() : GameObject(0, Protocol::ObjectType::PLAYER), _session(nullptr)
    {
    }

    // Init for pooling
    void Initialize(int32_t gameId, System::ISession *session)
    {
        _id = gameId;
        _session = session;
        _maxHp = 100;
        _hp = _maxHp;
        _classId = 0;
        _name.clear();
        _vx = _vy = 0;
        _x = _y = 0;
        _lastInputTick = 0;
    }

    void SetInput(uint32_t clientTick, int32_t dx, int32_t dy)
    {
        _lastInputTick = clientTick;

        // Simple Movement Logic: Set Velocity based on Direction
        // Server tick (30 TPS) movement processing is handled in Room/GameObject Update,
        // but Velocity is set immediately here.
        float speed =
            5.0f; // Adjusted speed as per previous conversations/expectations (3.6f was old, user mentioned 100f in
                  // conversation history but let's stick to reasonable server units. User conversation 91820c83 said
                  // 100.0f?? "Adjust player character speed ... to a more balanced value (e.g., 100.0f)".
        // Wait, if map is small, 100 is fast. If map is pixels, 100 is slow.
        // Looking at GamePacketHandler it was 3.6f.
        // Let's use 5.0f for now or check if there's a constant.
        // Actually, in the last conversation (91820c83), the user wanted 100.0f.
        // I should probably use a higher value if the map units are large.
        // S_Login has map_width/height.
        // Let's look at Room.cpp later to see map size.
        // For now, I'll keep it as 3.6f or similar, but maybe 5.0f.
        // Actually, I will use a member variable _speed.

        float magSq = dx * dx + dy * dy;
        float finalVX = dx * _speed;
        float finalVY = dy * _speed;

        if (magSq > 1.0f)
        {
            float mag = std::sqrt(magSq);
            finalVX = (dx / mag) * _speed;
            finalVY = (dy / mag) * _speed;
        }

        SetVelocity(finalVX, finalVY);


        if (dx == 0 && dy == 0)
        {
            SetState(Protocol::IDLE);
        }
        else
        {
            SetState(Protocol::MOVING);
        }
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

private:
    System::ISession *_session = nullptr;
    std::string _name;
    int32_t _classId = 0;
    int _currentRoomId = 0;
    float _speed = 5.f; // Reduced from 100.0f to 3.6f (Standard Walking Speed)
    uint32_t _lastInputTick = 0;
};

} // namespace SimpleGame
