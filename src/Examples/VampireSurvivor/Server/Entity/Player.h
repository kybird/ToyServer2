#pragma once
#include "Entity/GameObject.h"
#include "Math/Vector2.h"
#include "System/ILog.h"
#include "System/ISession.h"
#include <memory>
#include <string>
#include <vector>

namespace SimpleGame {

class DamageEmitter;

class Player : public GameObject
{
public:
    Player(int32_t gameId, System::ISession *session)
        : GameObject(gameId, Protocol::ObjectType::PLAYER), _session(session)
    {
    }

    // Default constructor for pooling
    Player();
    virtual ~Player();

    // Init for pooling
    void Initialize(int32_t gameId, System::ISession *session, int32_t hp, float speed);

    void ApplyInput(uint32_t clientTick, int32_t dx, int32_t dy);

    void TakeDamage(int32_t damage, Room *room) override;

    bool IsDead() const;

    // Reset for pooling
    void Reset();

    uint64_t GetSessionId() const;
    System::ISession *GetSession() const;

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

    // For Testing
    size_t GetEmitterCount() const;
    void AddEmitter(std::shared_ptr<DamageEmitter> emitter);
    void ClearEmitters();

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

    // Auto-Attack
    std::vector<std::shared_ptr<DamageEmitter>> _emitters;

    bool _isReady = false;
};

} // namespace SimpleGame
