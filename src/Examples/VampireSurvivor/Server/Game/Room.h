#pragma once
#include "Entity/Player.h"
#include <mutex>
#include <unordered_map>
#include <vector>

#include "System/ITimer.h"

#include "Game/Effect/EffectManager.h"
#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"
#include "Game/WaveManager.h"

namespace System {
class IPacket;
class IStrand;
} // namespace System

namespace SimpleGame {
class UserDB;
class DamageEmitter;
class CombatManager;

class Room : public System::ITimerListener, public std::enable_shared_from_this<Room>
{
    friend class CombatTest_ProjectileHitsMonster_Test;
    friend class CombatTest_MonsterDies_Test;
    friend class CombatTest_MonsterContactsPlayer_Test;
    friend class SwarmPerformanceTest_StressTest500Monsters_Test;
    friend class CombatTest_LinearEmitterHitsNearestMonster_Test;
    friend class CombatTest_LinearEmitterRespectsLifetime_Test;
    friend class CombatTest_MonsterKnockback_Test;
    friend class CombatTest_LinearEmitterSpawnsProjectile_Test;
    friend class CombatManager;
    friend class DamageEmitter;

public:
    Room(
        int roomId, std::shared_ptr<System::ITimer> timer, std::shared_ptr<System::IStrand> strand,
        std::shared_ptr<UserDB> userDB
    );
    virtual ~Room() override;

    void Enter(const std::shared_ptr<Player> &player);
    void Leave(uint64_t sessionId);
    void OnPlayerReady(uint64_t sessionId); // Called when client finishes loading
    void BroadcastPacket(const System::IPacket &pkt);
    void BroadcastSpawn(const std::vector<std::shared_ptr<GameObject>> &objects);
    void BroadcastDespawn(const std::vector<int32_t> &objectIds, const std::vector<int32_t> &pickerIds = {});

    // Game Loop
    void Start();
    void Stop();
    void OnTimer(uint32_t timerId, void *pParam) override;
    void Update(float deltaTime);

    int GetId() const
    {
        return _roomId;
    }
    size_t GetPlayerCount();
    static size_t GetMaxPlayers()
    {
        return 4;
    } // TODO: Make configurable
    bool IsPlaying() const
    {
        return _gameStarted;
    }

    std::shared_ptr<Player> GetNearestPlayer(float x, float y);
    std::vector<std::shared_ptr<Monster>> GetMonstersInRange(float x, float y, float radius);

    std::shared_ptr<System::IStrand> GetStrand() const
    {
        return _strand;
    }

    void StartGame(); // Start wave manager when first player enters
    void Reset();     // Reset room state when all players leave
    void HandleGameOver(bool isWin);

    void SetTitle(const std::string &title)
    {
        _title = title;
    }
    std::string GetTitle() const
    {
        return _title;
    }

    EffectManager &GetEffectManager()
    {
        return *_effectMgr;
    }
    ObjectManager &GetObjectManager()
    {
        return _objMgr;
    }

    float GetTotalRunTime() const
    {
        return _totalRunTime;
    }

    // Debug Commands
    void DebugAddExpToAll(int32_t exp);
    void DebugSpawnMonster(int32_t monsterId, int32_t count);
    void DebugToggleGodMode();

    // void HandleGameOver(bool isWin); (Removed duplicate)
    void UpdateObject(
        const std::shared_ptr<GameObject> &obj, float deltaTime, std::vector<Protocol::ObjectPos> &monsterMoves,
        std::vector<Protocol::ObjectPos> &playerMoves
    );
    void BroadcastPlayerMoves(const std::vector<Protocol::ObjectPos> &playerMoves);
    void SendPlayerAcks();
    bool CheckWinCondition() const;

private:
    int _roomId;
    std::string _title;
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players; // SessionID -> Player
    std::recursive_mutex _mutex;

    std::shared_ptr<System::ITimer> _timer;
    System::ITimer::TimerHandle _timerHandle = 0;
    std::shared_ptr<System::IStrand> _strand;

    // Game Logic Components
    ObjectManager _objMgr;
    SpatialGrid _grid{2000.0f}; // [Phase 1] Large cell for Full Broadcast
    WaveManager _waveMgr;
    std::shared_ptr<UserDB> _userDB;
    float _totalRunTime = 0.0f;
    uint32_t _serverTick = 0;
    float _debugBroadcastTimer = 0.0f; // Stage 0 Verification Timer

public:
    uint32_t GetServerTick() const
    {
        return _serverTick;
    }

private:
    bool _gameStarted = false; // Track if game has started
    bool _isGameOver = false;  // Track if game is over
    std::unique_ptr<CombatManager> _combatMgr;
    std::unique_ptr<EffectManager> _effectMgr;
};

} // namespace SimpleGame
