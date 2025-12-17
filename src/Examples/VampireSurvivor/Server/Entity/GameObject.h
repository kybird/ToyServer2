#pragma once
#include <cstdint>
#include "../../Protocol/game.pb.h"

namespace SimpleGame {

class Room; // Forward declaration

class GameObject {
public:
    GameObject(int32_t id, Protocol::ObjectType type) 
        : _id(id), _type(type) {}
    virtual ~GameObject() = default;

    virtual void Update(float dt, Room* room) {} // AI/Logic Update

    int32_t GetId() const { return _id; }
    Protocol::ObjectType GetType() const { return _type; }

    // Position & Movement
    float GetX() const { return _x; }
    float GetY() const { return _y; }
    void SetPos(float x, float y) { _x = x; _y = y; }
    
    float GetVX() const { return _vx; }
    float GetVY() const { return _vy; }
    void SetVelocity(float vx, float vy) { _vx = vx; _vy = vy; }

    float GetRadius() const { return _radius; }
    void SetRadius(float r) { _radius = r; }

    Protocol::ObjectState GetState() const { return _state; }
    void SetState(Protocol::ObjectState state) { _state = state; }

    // Stats
    int32_t GetHp() const { return _hp; }
    void SetHp(int32_t hp) { _hp = hp; }
    int32_t GetMaxHp() const { return _maxHp; }

protected:
    int32_t _id;
    Protocol::ObjectType _type;
    Protocol::ObjectState _state = Protocol::ObjectState::IDLE;

    float _x = 0.0f;
    float _y = 0.0f;
    float _vx = 0.0f;
    float _vy = 0.0f;
    float _radius = 0.5f;

    int32_t _hp = 100;
    int32_t _maxHp = 100;
};

} // namespace SimpleGame
