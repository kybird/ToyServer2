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
    DamageEmitter(int32_t skillId, std::shared_ptr<Player> owner);
    void Update(float dt, Room *room);
    bool IsExpired() const;

private:
    int32_t _skillId;
    std::weak_ptr<Player> _owner;

    int32_t _damage = 0;
    int32_t _typeId = 0;
    float _tickInterval = 1.0f;
    float _hitRadius = 1.0f;
    float _timer = 0.0f;

    std::string _emitterType = "AoE";
    int32_t _pierce = 1;
    int32_t _maxTargetsPerTick = 1;
    std::string _targetRule = "Nearest";
    float _lifeTime = 0.0f; // 0 = infinite
    float _elapsedTime = 0.0f;
    bool _active = true;
};

} // namespace SimpleGame
