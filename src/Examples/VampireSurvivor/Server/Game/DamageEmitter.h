#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace SimpleGame {

class Player;
class Room;

/**
 * @brief Handles periodic damage around the player based on skill templates.
 */
class DamageEmitter
{
public:
    DamageEmitter(int32_t skillId, std::shared_ptr<Player> owner, int32_t weaponId = 0, int32_t level = 1);
    ~DamageEmitter();
    void Update(float dt, Room *room);
    bool IsExpired() const;

    void SetLevel(int32_t level)
    {
        _level = level;
    }
    int32_t GetSkillId() const
    {
        return _skillId;
    }
    int32_t GetWeaponId() const
    {
        return _weaponId;
    }
    int32_t GetTypeId() const
    {
        return _typeId;
    }

private:
    enum class EmitterState { COOLING, ACTIVE };

    int32_t _skillId;
    int32_t _weaponId = 0;
    int32_t _level = 1;
    std::weak_ptr<Player> _owner;

    int32_t _damage = 0;
    int32_t _typeId = 0;
    float _tickInterval = 1.0f;
    float _hitRadius = 1.0f;
    float _timer = 0.0f;

    // Field/Persistent Stats
    float _activeDuration = 0.0f;
    float _dotInterval = 0.5f;
    float _dotTimer = 0.0f;
    EmitterState _state = EmitterState::COOLING;

    std::string _emitterType = "AoE";
    int32_t _pierce = 1;
    int32_t _maxTargetsPerTick = 1;
    std::string _targetRule = "Nearest";
    float _lifeTime = 0.0f; // 0 = infinite (Overall emitter life)
    float _elapsedTime = 0.0f;
    float _arcDegrees = 30.0f; // [New] Arc angle for Arc emitter type
    float _width = 1.0f;       // [New] Rectangular width
    float _height = 1.0f;      // [New] Rectangular height
    bool _active = true;

    std::unique_ptr<class IEmitter> _emitter;
};

} // namespace SimpleGame
