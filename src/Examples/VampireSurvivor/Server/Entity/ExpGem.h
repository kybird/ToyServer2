#pragma once
#include "Entity/GameObject.h"

namespace SimpleGame {

/**
 * @brief Experience Gem dropped by monsters.
 */
class ExpGem : public GameObject
{
public:
    ExpGem(int32_t id, int32_t expAmount) : GameObject(id, Protocol::ObjectType::ITEM), _expAmount(expAmount)
    {
    }

    ExpGem() : GameObject(0, Protocol::ObjectType::ITEM), _expAmount(0)
    {
    }

    void Initialize(int32_t id, float x, float y, int32_t expAmount)
    {
        _id = id;
        _x = x;
        _y = y;
        _expAmount = expAmount;
        _state = Protocol::ObjectState::IDLE;
        _vx = _vy = 0.0f;
    }

    int32_t GetExpAmount() const
    {
        return _expAmount;
    }

    bool IsPickedUp() const
    {
        return _isPickedUp;
    }
    void MarkAsPickedUp()
    {
        _isPickedUp = true;
    }

    int32_t GetPickerId() const
    {
        return _pickerId;
    }
    void SetPickerId(int32_t pickerId)
    {
        _pickerId = pickerId;
    }

private:
    int32_t _expAmount = 0;
    bool _isPickedUp = false;
    int32_t _pickerId = 0;
};

} // namespace SimpleGame
