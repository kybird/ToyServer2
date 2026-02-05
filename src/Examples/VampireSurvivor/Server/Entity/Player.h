#pragma once
#include "Entity/GameObject.h"
#include "Math/Vector2.h"
#include "System/ISession.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace SimpleGame {

class DamageEmitter;
class PlayerInventory;
struct LevelUpOption;

class Player : public GameObject
{
public:
    Player(int32_t gameId, uint64_t sessionId);

    // Default constructor for pooling
    Player();
    virtual ~Player() override;

    // Init for pooling
    void Initialize(int32_t gameId, uint64_t sessionId, int32_t hp, float speed);

    void ApplyInput(uint32_t clientTick, int32_t dx, int32_t dy);

    void TakeDamage(int32_t damage, Room *room) override;

    bool IsDead() const;

    // Reset for pooling
    void Reset();

    uint64_t GetSessionId() const;

    void SetName(const std::string &name);
    const std::string &GetName() const;

    // Class system?
    void SetClassId(int32_t classId);
    int32_t GetClassId() const;

    void SetRoomId(int roomId);
    int GetRoomId() const;

    Vector2 GetFacingDirection() const;

    void ApplySkills(const std::vector<std::pair<int, int>> &skills);

    uint32_t GetLastProcessedClientTick() const;

    // Experience & Level
    int32_t GetExp() const;
    int32_t GetMaxExp() const;
    int32_t GetLevel() const;

    void AddExp(int32_t amount, Room *room);
    void RefreshInventoryEffects();

    // Passive Stats Calculation
    float GetDamageMultiplier() const;
    float GetMaxHpMultiplier() const;
    float GetMovementSpeedMultiplier() const;
    float GetCooldownMultiplier() const;
    float GetAreaMultiplier() const;
    float GetDurationMultiplier() const;
    int32_t GetAdditionalProjectileCount() const;
    int32_t GetAdditionalPierceCount() const;

    // Invincibility
    bool IsInvincible(float currentTime) const;
    void SetInvincible(float untilTime);

    // Level Up State (Invincible + Input Disabled)
    bool IsLevelingUp() const
    {
        return _isLevelingUp;
    }
    void EnterLevelUpState(Room *room);
    void ExitLevelUpState(Room *room);

    // Auto-Attack
    void Update(float dt, Room *room) override;
    void AddDefaultSkills(const std::vector<int32_t> &skillIds);

    // Ready State (Control Broadcast)
    bool IsReady() const
    {
        return _isReady;
    }
    void SetReady(bool ready)
    {
        _isReady = ready;
    }

    // Level Up System
    PlayerInventory &GetInventory();
    const std::vector<LevelUpOption> &GetPendingLevelUpOptions() const;
    void SetPendingLevelUpOptions(const std::vector<LevelUpOption> &options);
    void ClearPendingLevelUpOptions();

    // For Testing
    size_t GetEmitterCount() const;
    void AddEmitter(std::shared_ptr<DamageEmitter> emitter);
    void ClearEmitters();

    // Debug
    void SetGodMode(bool enable);
    bool IsGodMode() const;

    // Critical Hit
    float GetCriticalChance() const;
    float GetCriticalDamageMultiplier() const;

private:
    uint64_t _sessionId = 0;
    std::string _name;
    int32_t _classId = 0;
    int _currentRoomId = 0;
    float _speed = 5.f;
    uint32_t _lastInputTick = 0;
    float _facingDirX = 1.0f;
    float _facingDirY = 0.0f;

    // HP
    int32_t _baseMaxHp = 100;

    // Experience & Level
    int32_t _exp = 0;
    int32_t _maxExp = 100;
    int32_t _level = 1;

    float _invincibleUntil = 0.0f;
    bool _isLevelingUp = false;
    float _levelUpStartedAt = 0.0f;

    // Auto-Attack
    std::vector<std::shared_ptr<DamageEmitter>> _emitters;

    // Level Up System
    std::unique_ptr<PlayerInventory> _inventory;
    std::vector<LevelUpOption> _pendingLevelUpOptions;
    std::set<int32_t> _slowedMonsterIds;

    bool _isReady = false;
    bool _godMode = false;
};

} // namespace SimpleGame
